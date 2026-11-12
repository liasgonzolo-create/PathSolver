"""The table of removed leveldp switches: its text source, its generated header, and the refusal it feeds.

    python tools/test_removed_flags.py
    LEVELDP_EXE=<path to leveldp.exe> python tools/test_removed_flags.py     # also runs the refusal

(a) dp/src/dp/removed_flags.gen.hpp is what tools/gen_removed_flags.py makes of dp/removed_flags.txt.
(b) No spelling in the text is still parsed: none appears as a quoted string in the solver (dp/src), the mod (src)
    or the tools that start it (py, mcp, tools, scripts). The refusal comes first, so a parse branch left behind
    would be dead code that reads as a live switch.
(c) The refusal reads as it always has: "leveldp: <arg> was removed in <sha>; the behaviour is fixed (since <sha>)",
    on stdout and on stderr, exit status 2. Pinned in cli.hpp's text, and, when an exe is at hand, by running it.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))

import gen_removed_flags as gen  # noqa: E402
from check_code_only_diff import strip_comments  # noqa: E402

CLI = ROOT / "dp" / "src" / "dp" / "cli.hpp"
MESSAGE = "leveldp: {arg} was removed in {removed}; the behaviour is fixed (since {since})"
SCAN_DIRS = [("dp/src", (".hpp", ".cpp", ".h")), ("src", (".hpp", ".cpp", ".h")),
             ("py", (".py",)), ("mcp", (".py",)), ("tools", (".py", ".ps1")), ("scripts", (".ps1", ".py"))]


def rows():
    return gen.parse(gen.SOURCE.read_text(encoding="utf-8"))


class Source(unittest.TestCase):
    def test_parses_and_is_not_empty(self):
        self.assertGreater(len(rows()), 0)

    def test_every_line_has_a_reason(self):
        for spelling, _, _, reason in rows():
            self.assertTrue(reason, spelling)


class GeneratedHeader(unittest.TestCase):
    def test_a_header_is_what_the_text_makes(self):
        want = gen.render(rows())
        got = gen.OUT.read_text(encoding="utf-8")
        self.assertEqual(got, want, "stale: run python tools/gen_removed_flags.py")

    def test_a_script_check_agrees(self):
        r = subprocess.run([sys.executable, str(HERE / "gen_removed_flags.py"), "--check"], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)


def source_files(root=ROOT):
    for folder, exts in SCAN_DIRS:
        base = root / folder
        if not base.is_dir():
            continue
        for p in base.rglob("*"):
            if p.is_file() and p.suffix in exts and not p.name.endswith(".gen.hpp") \
                    and p.name != "test_removed_flags.py" and "build" not in p.relative_to(root).parts[0]:
                yield p


def scan(root, spellings):
    """Every "<spelling> in <file>" where a removed spelling is a quoted string in the code under root."""
    quoted = {s: (f'"{s}"', f"'{s}'") for s in spellings}
    found: list[str] = []
    for p in source_files(root):
        text = p.read_text(encoding="utf-8", errors="replace")
        cand = [s for s, qs in quoted.items() if any(q in text for q in qs)]
        if not cand:
            continue
        # only a hit that survives removing the comments counts (C++ files; a script's comment is code to it)
        # (strip_comments returns the code's lines; a list would make every `q in code` below an element test)
        code = "\n".join(strip_comments(text)) if p.suffix in (".hpp", ".cpp", ".h") else text
        for s in cand:
            if any(q in code for q in quoted[s]):
                found.append(f"{s} in {p.relative_to(root).as_posix()}")
    return found


class NotParsedAnyMore(unittest.TestCase):
    def test_b_no_removed_spelling_is_a_string_in_the_tree(self):
        found = scan(ROOT, [s for s, _, _, _ in rows()])
        self.assertEqual(found, [], "still parsed or passed: remove it from the text or from the code")


class ScanControls(unittest.TestCase):
    """The scan above is only evidence while it can fail. These run it over a tree built here: a removed spelling that
    is a quoted string in C++ code must be found (positive control); one that is only in a comment must not (negative).
    The scan passed for months while it looked at no C++ file at all; this is what would have said so."""

    def tree(self, files):
        d = Path(tempfile.mkdtemp())
        for name, body in files.items():
            f = d / name
            f.parent.mkdir(parents=True, exist_ok=True)
            f.write_text(body, encoding="utf-8")
        return d

    def test_positive_a_quoted_spelling_in_cpp_code_is_found(self):
        d = self.tree({"dp/src/dp/a.hpp": 'if (!std::strcmp(argv[i], "--gone")) g_x = true;\n',
                       "src/mod/b.cpp": 'const char* k = "--no-gone";\n',
                       "py/c.py": 'args = ["--gone"]\n'})
        self.assertEqual(sorted(scan(d, ["--gone", "--no-gone"])),
                         ["--gone in dp/src/dp/a.hpp", "--gone in py/c.py", "--no-gone in src/mod/b.cpp"])

    def test_negative_a_spelling_only_in_a_comment_is_not_found(self):
        d = self.tree({"dp/src/dp/a.hpp": '// "--gone" was parsed here\n/* "--gone" and "--no-gone" */\nint x = 0;   // "--gone"\n'})
        self.assertEqual(scan(d, ["--gone", "--no-gone"]), [])

    def test_negative_a_live_spelling_is_not_found(self):
        d = self.tree({"dp/src/dp/a.hpp": 'if (!std::strcmp(argv[i], "--alive")) g_y = true;\n'})
        self.assertEqual(scan(d, ["--gone"]), [])


FRAGMENT = re.compile(r'"\s*\n\s*"')


class Refusal(unittest.TestCase):
    def test_c_wording_in_cli_hpp(self):
        text = CLI.read_text(encoding="utf-8")
        at = text.index("for (const auto& r : kRemovedFlags)")
        block = FRAGMENT.sub("", text[at:text.index("return 2;", at)])
        self.assertIn('std::printf("leveldp: %s was removed in %s; the behaviour is fixed (since %s)\\n", argv[i], r[1], r[2]);', block)
        self.assertIn('std::fprintf(stderr, "leveldp: %s was removed in %s; the behaviour is fixed (since %s)\\n", argv[i], r[1], r[2]);', block)

    @unittest.skipUnless(os.environ.get("LEVELDP_EXE") and Path(os.environ.get("LEVELDP_EXE", "")).is_file(),
                         "set LEVELDP_EXE to run the refusal")
    def test_c_wording_from_the_exe(self):
        exe = os.environ["LEVELDP_EXE"]
        table = rows()
        plain = next(r for r in table if not r[0].startswith("--no-") and not r[0].endswith("="))
        off = next(r for r in table if r[0].startswith("--no-"))
        prefix = next((r for r in table if r[0].endswith("=")), None)
        picks = [(plain, plain[0]), (off, off[0])] + ([(prefix, prefix[0] + "5")] if prefix else [])
        with tempfile.TemporaryDirectory() as d:
            level = str(Path(d) / "no_such_level.txt")
            for (spelling, removed, since, _), arg in picks:
                r = subprocess.run([exe, level, arg], capture_output=True, text=True, cwd=d)
                want = MESSAGE.format(arg=arg, removed=removed, since=since)
                self.assertEqual(r.returncode, 2, arg)
                # stdout has the fpenv line first (cliMain prints it before anything else); the refusal is its last line
                self.assertEqual(r.stdout.splitlines()[-1], want, arg)
                self.assertEqual(r.stderr, want + "\n", arg)

    @unittest.skipUnless(os.environ.get("LEVELDP_EXE") and Path(os.environ.get("LEVELDP_EXE", "")).is_file(),
                         "set LEVELDP_EXE to run the refusal")
    def test_c_a_live_spelling_is_not_refused(self):
        # a spelling that is not in the text is not refused by name (it fails later, on the missing level)
        exe = os.environ["LEVELDP_EXE"]
        with tempfile.TemporaryDirectory() as d:
            r = subprocess.run([exe, str(Path(d) / "no_such_level.txt"), "--cap", "10"], capture_output=True, text=True, cwd=d)
            self.assertNotIn("was removed in", r.stdout + r.stderr)


if __name__ == "__main__":
    unittest.main()
