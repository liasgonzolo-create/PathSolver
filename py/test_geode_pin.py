"""The Geode pin, the update-relaunch detection and the manifest's loader field, checked without GD.

What is pinned:
  * a worker whose Geode files are the pin's passes; a different Geode.dll, a missing file and a staged
    geode/update each refuse the launch (GeodeMismatch, which is not a WorkerError);
  * GDSOLVER_ALLOW_GEODE_DRIFT=1 turns the refusal into a warning, and no pin file means no check;
  * geode_identity reads the loader's version file and the hash of Geode.dll;
  * a game started without --geode:<mod>.data-root is what an applied update leaves behind, and is told apart;
  * cold_manifest.compare refuses two runs on different loaders unless `geode` is allowed, and says nothing
    about a run that predates the field.

usage: python py/test_geode_pin.py   (exit 1 on the first failure)
"""
import contextlib
import hashlib
import io
import json
import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cold_manifest as CM          # noqa: E402
from gdtas import gdbase            # noqa: E402
from gdtas import worker as W       # noqa: E402
from gdtas.paths import MOD_ID      # noqa: E402


def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def make_worker(root: Path, files: dict[str, bytes], version: str = "v9.9.9") -> None:
    root.mkdir(parents=True, exist_ok=True)
    for n, b in files.items():
        (root / n).write_bytes(b)
    res = root / "geode" / "resources" / "geode.loader"
    res.mkdir(parents=True, exist_ok=True)
    (res / "version").write_text(version, encoding="utf-8")


def expect_raises(exc, fn, what):
    try:
        fn()
    except exc:
        return
    except Exception as e:                      # noqa: BLE001
        raise SystemExit(f"FAIL {what}: raised {type(e).__name__}: {e}")
    raise SystemExit(f"FAIL {what}: did not raise {exc.__name__}")


def main() -> int:
    good = {"Geode.dll": b"loader-9", "GeodeUpdater.exe": b"updater-9", "XInput1_4.dll": b"xinput-9"}
    tmp = Path(tempfile.mkdtemp(prefix="geode_pin_test_"))
    pin_file = tmp / "geode_pin.json"
    pin_file.write_text(json.dumps({"version": "v9.9.9", "files": {k: sha(v) for k, v in good.items()}}), encoding="utf-8")
    gdbase.GEODE_PIN = pin_file
    os.environ.pop(gdbase.ALLOW_GEODE_DRIFT, None)

    ok = tmp / "worker-ok"
    make_worker(ok, good)
    gdbase.check_geode(ok)
    ident = gdbase.geode_identity(ok)
    assert ident == {"version": "v9.9.9", "dll_sha256": sha(good["Geode.dll"])}, ident

    old = tmp / "worker-old"
    make_worker(old, {**good, "Geode.dll": b"loader-8"}, "v9.9.8")
    expect_raises(gdbase.GeodeMismatch, lambda: gdbase.check_geode(old), "a different Geode.dll")
    assert not issubclass(gdbase.GeodeMismatch, W.WorkerError), "a retry cannot fix a loader mismatch"
    assert any("Geode.dll" in p for p in gdbase.geode_problems(old)), gdbase.geode_problems(old)

    gone = tmp / "worker-gone"
    make_worker(gone, {k: v for k, v in good.items() if k != "XInput1_4.dll"})
    expect_raises(gdbase.GeodeMismatch, lambda: gdbase.check_geode(gone), "a missing file")

    staged = tmp / "worker-staged"
    make_worker(staged, good)
    (staged / "geode" / "update").mkdir()
    expect_raises(gdbase.GeodeMismatch, lambda: gdbase.check_geode(staged), "a staged update")

    os.environ[gdbase.ALLOW_GEODE_DRIFT] = "1"
    err = io.StringIO()
    with contextlib.redirect_stderr(err):
        gdbase.check_geode(old)
    assert "warning" in err.getvalue() and gdbase.geode_drift_allowed()
    os.environ.pop(gdbase.ALLOW_GEODE_DRIFT)

    gdbase.GEODE_PIN = tmp / "no_such_pin.json"
    gdbase.check_geode(old)                       # nothing pinned, nothing checked
    assert gdbase.geode_problems(old) == []
    gdbase.GEODE_PIN = pin_file

    # an applied update leaves a game with no data-root argument
    key = f"--geode:{MOD_ID}.data-root=C:/w/session/data"
    stray = W._argless([(11, ["GeometryDash-worker-1.exe", key]), (12, ["GeometryDash-worker-1.exe"]),
                        (13, ["GeometryDash-worker-1.exe", "--geode:other.mod.setting=1"])])
    assert stray == [12, 13], stray

    # the manifest: a run on another loader is refused unless it is named
    a = {"head": "h", "mod_sha256": "m", "cfg": [], "levels": {}, "geode": {"version": "v1", "dll_sha256": "aa" * 32}}
    b = {"head": "h", "mod_sha256": "m", "cfg": [], "levels": {}, "geode": {"version": "v2", "dll_sha256": "bb" * 32}}
    with contextlib.redirect_stdout(io.StringIO()) as out:
        assert CM.compare(a, b, set(), False) == 2
        assert "geode: v1 " in out.getvalue() and "--allow geode" in out.getvalue(), out.getvalue()
    with contextlib.redirect_stdout(io.StringIO()):
        assert CM.compare(a, b, {"geode"}, False) == 0
    old_run = {"head": "h", "mod_sha256": "m", "cfg": [], "levels": {}}
    with contextlib.redirect_stdout(io.StringIO()) as out:
        assert CM.compare(a, old_run, set(), False) == 0
        assert "recorded by only one" in out.getvalue(), out.getvalue()
    with contextlib.redirect_stdout(io.StringIO()):
        assert CM.compare(a, dict(a), set(), False) == 0

    print("test_geode_pin: all ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
