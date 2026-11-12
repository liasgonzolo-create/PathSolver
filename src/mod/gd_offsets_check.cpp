// Compile-time check of src/mod/gd_offsets.hpp: every raw offset must be where the bindings put
// the member it stands for, on the platform being built. Nothing here runs.
//
// On Windows this holds the literals the code has always used against the bindings' layout. On
// android64 it holds the values measured on the Android binary against the same bindings as
// the Android compiler lays them out, which is the check that the layout the measurement
// assumed is the one the mod is built with. The RVAs of the three seeds are not members and
// have no check here; tools/android_offsets.py finds them in the binary.
#include <Geode/Geode.hpp>

#include <cstddef>

#include "mod/gd_offsets.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#endif

namespace {
using namespace gdoff;
using cocos2d::CCPoint;

constexpr std::size_t kGameState = offsetof(GJBaseGameLayer, m_gameState);
#define STATE(m) (kGameState + offsetof(GJGameState, m))

// GJBaseGameLayer
static_assert(kLayerCameraZoom == STATE(m_cameraZoom));
static_assert(kLayerCameraOffsetY == STATE(m_cameraOffset) + offsetof(CCPoint, y));
static_assert(kLayerUnkPoint17Y == STATE(m_unkPoint17) + offsetof(CCPoint, y));
static_assert(kLayerUnkBool1 == STATE(m_unkBool1));
static_assert(kLayerFreeMode == STATE(m_isFreeMode));
static_assert(kLayerCurrentChannel == STATE(m_currentChannel));
static_assert(kLayerSpawnChannel0 == STATE(m_spawnChannelRelated0));
static_assert(kLayerSpawnChannel1 == STATE(m_spawnChannelRelated1));
static_assert(kLayerUnkBool10 == STATE(m_unkBool10));
static_assert(kLayerLevelFlipping == STATE(m_levelFlipping));
static_assert(kLayerDualMode == STATE(m_isDualMode));
static_assert(kLayerTargetGroupsArray == offsetof(GJBaseGameLayer, m_targetGroupsArray));
static_assert(kLayerObjectParent == offsetof(GJBaseGameLayer, m_objectParent));
static_assert(kLayerAboveShaderParent == offsetof(GJBaseGameLayer, m_aboveShaderParent));
static_assert(kLayerObjectLayer == offsetof(GJBaseGameLayer, m_objectLayer));
static_assert(kLayerInShaderObjectLayer == offsetof(GJBaseGameLayer, m_inShaderObjectLayer));
static_assert(kLayerAboveShaderObjectLayer == offsetof(GJBaseGameLayer, m_aboveShaderObjectLayer));
static_assert(kLayerBatchNodes == offsetof(GJBaseGameLayer, m_batchNodes));
static_assert(kLayerVarianceValues == offsetof(GJBaseGameLayer, m_varianceValues));
static_assert(kLayerEnterEasingValues == offsetof(GJBaseGameLayer, m_enterEasingValues));
static_assert(kLayerShaderLayer == offsetof(GJBaseGameLayer, m_shaderLayer));
static_assert(kLayerRandomSeed == offsetof(GJBaseGameLayer, m_randomSeed));
static_assert(kLayerNonEffectObjects == offsetof(GJBaseGameLayer, m_nonEffectObjects));
static_assert(kLayerCollisionBlockSections == offsetof(GJBaseGameLayer, m_collisionBlockSections));
static_assert(kLayerNonEffectObjectsSizes == offsetof(GJBaseGameLayer, m_nonEffectObjectsSizes));
static_assert(kLayerCollisionBlockSectionSizes
              == offsetof(GJBaseGameLayer, m_collisionBlockSectionSizes));
static_assert(kLayerMaxGameplayY == offsetof(GJBaseGameLayer, m_maxGameplayY));

// PlayerObject
static_assert(kPlayerSpriteWidthScale == offsetof(PlayerObject, m_spriteWidthScale));
static_assert(kPlayerSpriteHeightScale == offsetof(PlayerObject, m_spriteHeightScale));
static_assert(kPlayerSlopeStartTime == offsetof(PlayerObject, m_slopeStartTime));
static_assert(kPlayerCollisionLogBottom == offsetof(PlayerObject, m_collisionLogBottom));
static_assert(kPlayerCollisionLogLeft == offsetof(PlayerObject, m_collisionLogLeft));
static_assert(kPlayerCurrentSlope2 == offsetof(PlayerObject, m_currentSlope2));
static_assert(kPlayerCollidedObject == offsetof(PlayerObject, m_collidedObject));
static_assert(kPlayerLastGroundObject == offsetof(PlayerObject, m_lastGroundObject));
static_assert(kPlayerCollidingWithSlope == offsetof(PlayerObject, m_isCollidingWithSlope));
static_assert(kPlayerCurrentSlope == offsetof(PlayerObject, m_currentSlope));
static_assert(kPlayerSlopeFlipGravity == offsetof(PlayerObject, m_slopeFlipGravityRelated));
static_assert(kPlayerRotationSpeed == offsetof(PlayerObject, m_rotationSpeed));
static_assert(kPlayerLastSpiderFlipTime == offsetof(PlayerObject, m_lastSpiderFlipTime));
static_assert(kPlayerAccelerating == offsetof(PlayerObject, m_isAccelerating));
static_assert(kPlayerCollidedBottomMaxY == offsetof(PlayerObject, m_collidedBottomMaxY));
static_assert(kPlayerCollidedLeftMaxX == offsetof(PlayerObject, m_collidedLeftMaxX));
static_assert(kPlayerJumpBuffered == offsetof(PlayerObject, m_jumpBuffered));
static_assert(kPlayerStateRingJump == offsetof(PlayerObject, m_stateRingJump));
static_assert(kPlayerStateJumpBuffered == offsetof(PlayerObject, m_stateJumpBuffered));
static_assert(kPlayerStateRingJump2 == offsetof(PlayerObject, m_stateRingJump2));
static_assert(kPlayerTouchedRing == offsetof(PlayerObject, m_touchedRing));
static_assert(kPlayerTouchedRing + 1 == offsetof(PlayerObject, m_touchedCustomRing));
static_assert(kPlayerTouchedRing + 2 == offsetof(PlayerObject, m_touchedGravityPortal));
static_assert(kPlayerOnSlope == offsetof(PlayerObject, m_isOnSlope));
static_assert(kPlayerUpsideDownSlope == offsetof(PlayerObject, m_maybeUpsideDownSlope));
static_assert(kPlayerUpsideDown == offsetof(PlayerObject, m_isUpsideDown));
static_assert(kPlayerOnGround == offsetof(PlayerObject, m_isOnGround));
static_assert(kPlayerGoingLeft == offsetof(PlayerObject, m_isGoingLeft));
static_assert(kPlayerSideways == offsetof(PlayerObject, m_isSideways));
static_assert(kPlayerDashing == offsetof(PlayerObject, m_isDashing));
static_assert(kPlayerFlagsDumpBegin == offsetof(PlayerObject, m_collidedObject));
static_assert(kPlayerFlagsDumpEnd == offsetof(PlayerObject, m_scaleXRelatedTime));
static_assert(kPlayerLocked == offsetof(PlayerObject, m_isLocked));
static_assert(kPlayerPosition == offsetof(PlayerObject, m_position));
static_assert(kPlayerTotalTime == offsetof(PlayerObject, m_totalTime));
static_assert(kPlayerPlatformer == offsetof(PlayerObject, m_isPlatformer));
static_assert(kPlayerStateNoAutoJump == offsetof(PlayerObject, m_stateNoAutoJump));
static_assert(kPlayerStateDartSlide == offsetof(PlayerObject, m_stateDartSlide));
static_assert(kPlayerStateHitHead == offsetof(PlayerObject, m_stateHitHead));
static_assert(kPlayerStateFlipGravity == offsetof(PlayerObject, m_stateFlipGravity));
static_assert(kPlayerGravityMod == offsetof(PlayerObject, m_gravityMod));
static_assert(kPlayerStateForce == offsetof(PlayerObject, m_stateForce));
static_assert(kPlayerOutOfBounds == offsetof(PlayerObject, m_isOutOfBounds));
static_assert(kPlayerIgnoreDamage == offsetof(PlayerObject, m_ignoreDamage));

// GameObject and subclasses
static_assert(kObjOuterSectionIndex == offsetof(GameObject, m_outerSectionIndex));
static_assert(kObjGroupDisabled == offsetof(GameObject, m_isGroupDisabled));
static_assert(kObjPositionXOffset == offsetof(GameObject, m_positionXOffset));
static_assert(kObjPositionYOffset == offsetof(GameObject, m_positionYOffset));
static_assert(kObjTempOffsetXRelated == offsetof(GameObject, m_tempOffsetXRelated));
static_assert(kObjGlowSprite == offsetof(GameObject, m_glowSprite));
static_assert(kObjUniqueID == offsetof(GameObject, m_uniqueID));
static_assert(kObjVarianceIndex == offsetof(GameObject, m_varianceIndex));
static_assert(kObjAreaStamp == offsetof(GameObject, m_unk4C8));
static_assert(kObjPassable == offsetof(GameObject, m_isPassable));
static_assert(kObjAreaMoveSkip == offsetof(GameObject, m_unk508));
static_assert(kRingClaimTouch == offsetof(RingObject, m_claimTouch));
static_assert(kCheckpointObject == offsetof(CheckpointObject, m_physicalCheckpointObject));

static_assert(kEnterStartAngle == offsetof(EnterEffectObject, m_startAngle));
static_assert(kEnterAnglePosition == offsetof(EnterEffectObject, m_anglePosition));
static_assert(kEnterRelative == offsetof(EnterEffectObject, m_relative));
static_assert(kEnterRelativeFade == offsetof(EnterEffectObject, m_relativeFade));
static_assert(kEnterEasingInType == offsetof(EnterEffectObject, m_easingInType));
static_assert(kEnterEasingInRate == offsetof(EnterEffectObject, m_easingInRate));
static_assert(kEnterEasingInBuffer == offsetof(EnterEffectObject, m_easingInBuffer));
static_assert(kEnterEasingOutType == offsetof(EnterEffectObject, m_easingOutType));
static_assert(kEnterEasingOutRate == offsetof(EnterEffectObject, m_easingOutRate));
static_assert(kEnterEasingOutBuffer == offsetof(EnterEffectObject, m_easingOutBuffer));
static_assert(kEnterDirectionType == offsetof(EnterEffectObject, m_directionType));
static_assert(kEnterXYMode == offsetof(EnterEffectObject, m_xyMode));
static_assert(kEnterEaseOutEnabled == offsetof(EnterEffectObject, m_easeOutEnabled));
static_assert(kEnterInbound == offsetof(EnterEffectObject, m_inbound));
static_assert(kEnterDontEditAreaParent == offsetof(EnterEffectObject, m_dontEditAreaParent));

static_assert(kInstLength == offsetof(EnterEffectInstance, m_length));
static_assert(kInstLengthVariance == offsetof(EnterEffectInstance, m_lengthVariance));
static_assert(kInstOffsetVariance == offsetof(EnterEffectInstance, m_offsetVariance));
static_assert(kInstOffsetYVariance == offsetof(EnterEffectInstance, m_offsetYVariance));
static_assert(kInstModFront == offsetof(EnterEffectInstance, m_modFront));
static_assert(kInstModBack == offsetof(EnterEffectInstance, m_modBack));
static_assert(kInstDeadzone == offsetof(EnterEffectInstance, m_deadzone));
static_assert(kInstMoveDistance == offsetof(EnterEffectInstance, m_moveDistance));
static_assert(kInstMoveDistanceVariance == offsetof(EnterEffectInstance, m_moveDistanceVariance));
static_assert(kInstMoveAngle == offsetof(EnterEffectInstance, m_moveAngle));
static_assert(kInstMoveAngleVariance == offsetof(EnterEffectInstance, m_moveAngleVariance));
static_assert(kInstMoveX == offsetof(EnterEffectInstance, m_moveX));
static_assert(kInstMoveXVariance == offsetof(EnterEffectInstance, m_moveXVariance));
static_assert(kInstMoveY == offsetof(EnterEffectInstance, m_moveY));
static_assert(kInstMoveYVariance == offsetof(EnterEffectInstance, m_moveYVariance));
static_assert(kInstScaleXVariance == offsetof(EnterEffectInstance, m_scaleXVariance));
static_assert(kInstScaleYVariance == offsetof(EnterEffectInstance, m_scaleYVariance));
static_assert(kInstRotateVariance == offsetof(EnterEffectInstance, m_unk074));
static_assert(kInstGameObject == offsetof(EnterEffectInstance, m_gameObject));
static_assert(kInstTargetGroupIndex == offsetof(EnterEffectInstance, m_targetGroupIndex));

// Menu layers
static_assert(kLevelInfoBusy == offsetof(LevelInfoLayer, m_isBusy));
static_assert(kLevelInfoTransitionDone == offsetof(LevelInfoLayer, m_enterTransitionFinished));
static_assert(kLevelPageBusy == offsetof(LevelPage, m_isBusy));
static_assert(kEditLevelExiting == offsetof(EditLevelLayer, m_exiting));
static_assert(kLevelRequiredCoins == offsetof(GJGameLevel, m_requiredCoins));

#undef STATE
// Sizes. A class the mod allocates through an inline create() is allocated at sizeof(), and the game's constructor then writes every byte the game has.
#if GEODE_COMP_GD_VERSION >= 22090 && defined(GEODE_IS_WINDOWS)
// 2.209's GJGameLevel is 0x840 bytes. Bindings that stop short of it make the first level the mod creates itself (the slice's copy) overflow the heap.
static_assert(sizeof(GJGameLevel) == 0x840, "the bindings' GJGameLevel must be the game's 0x840 bytes: GJGameLevel::create() allocates it inline");
// The bases the mod derives from (a derived class puts its own members at sizeof(base), and the base's constructor, in the game's or cocos' code, writes
// the real size) and the classes it creates itself. Each is the size the class's own create() allocates in 2.209 (libcocos2d.dll for cocos2d, the exe for
// the rest); CCObject's is the highest field its constructor writes.
static_assert(sizeof(cocos2d::CCObject) == 0x38);
static_assert(sizeof(cocos2d::CCNode) == 0x140);
static_assert(sizeof(cocos2d::CCNodeRGBA) == 0x158);
static_assert(sizeof(cocos2d::CCLayer) == 0x198);
static_assert(sizeof(cocos2d::CCLayerColor) == 0x220);
static_assert(sizeof(cocos2d::CCMenuItem) == 0x178);
static_assert(sizeof(FLAlertLayer) == 0x298);
static_assert(sizeof(CCMenuItemSpriteExtra) == 0x208);
static_assert(sizeof(ButtonSprite) == 0x2c8);
static_assert(sizeof(GJGameState) == 0x6d0);
// A struct the mod allocates and the game fills (psnap's EMSnap::full): GJEffectManager::saveToState reaches +0x338 (4 bytes) in 2.209, nothing there in 2.2081.
static_assert(sizeof(EffectManagerState) == 0x340);
// The element psnap keeps a vector of and assigns to and from the game's own vector (updatePulseEffects divides by 72 in both versions).
static_assert(sizeof(PulseEffectAction) == 72);
// The objects psnap and cpflight copy as raw bytes: the size the game frees (the scalar-deleting destructor in slot 1 of its vtable) is the size the copy must cover.
static_assert(sizeof(PlayerObject) == 0xc50);
static_assert(sizeof(GameObject) == 0x550);
static_assert(sizeof(EffectGameObject) == 0x740);
static_assert(sizeof(GJBaseGameLayer) == GD209(0x3978, 0x3938));   // the update took 0x40 bytes out at m_effectManager
static_assert(sizeof(PlayLayer) == GD209(0x3d20, 0x3ce0));
static_assert(sizeof(GJEffectManager) == 0x828);
#endif

}  // namespace

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
