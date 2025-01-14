# Bit Blot Game Engine sources...
set(BBGE_SRCS
    ActionInput.cpp
    ActionInput.h
    ActionMapper.cpp
    ActionMapper.h
    ActionSet.cpp
    ActionSet.h
    ActionStatus.cpp
    ActionStatus.h
    AfterEffect.cpp
    AfterEffect.h
    Base.cpp
    Base.h
    BaseText.h
    bithacks.h
    BitmapFont.cpp
    BitmapFont.h
    CMakeLists.txt
    Core.cpp
    Core.h
    Interpolators.cpp
    Interpolators.h
    DarkLayer.cpp
    DarkLayer.h
    DataStructures.cpp
    DataStructures.h
    DebugFont.cpp
    DebugFont.h
    Emitter.cpp
    EngineEnums.h
    Event.cpp
    Event.h
    FmodOpenALBridge.cpp
    FmodOpenALBridge.h
    FrameBuffer.cpp
    FrameBuffer.h
    GameKeyNames.cpp
    GameKeyNames.h
    GameKeys.h
    GLLoad.cpp
    GLLoad.h
    Gradient.cpp
    Gradient.h
    Image.cpp
    Image.h
    Joystick.cpp
    Joystick.h
    LensFlare.cpp
    LensFlare.h
    Localization.cpp
    Localization.h
    MathFunctions.h
    MT.cpp
    MT.h
    OpenGLStubs.h
    OSFunctions.cpp
    OSFunctions.h
    ParticleEffect.cpp
    ParticleManager.cpp
    Particles.cpp
    Particles.h
    Precacher.cpp
    Precacher.h
    Quad.cpp
    Quad.h
    QuadGrid.cpp
    QuadGrid.h
    ReadXML.cpp
    ReadXML.h
    Rect.h
    Refcounted.h
    RenderBase.cpp
    RenderBase.h
    RenderGrid.cpp
    RenderGrid.h
    RenderObject.cpp
    RenderObject.h
    RenderObject_inline.h
    RenderObjectLayer.cpp
    RenderRect.cpp
    RenderState.cpp
    RenderState.h
    RoundedRect.cpp
    RoundedRect.h
    ScreenTransition.cpp
    ScreenTransition.h
    ScriptObject.cpp
    ScriptObject.h
    Shader.cpp
    Shader.h
    SimpleIStringStream.h
    SkeletalSprite.cpp
    SkeletalSprite.h
    Slider.cpp
    Slider.h
    SoundManager.cpp
    SoundManager.h
    SpawnParticleData.cpp
    SplineGrid.cpp
    SplineGrid.h
    StateMachine.cpp
    StateMachine.h
    StateManager.cpp
    StateManager.h
    StringBank.cpp
    StringBank.h
    Texture.cpp
    Texture.h
    TextureMgr.cpp
    TextureMgr.h
    TTFFont.cpp
    TTFFont.h
    Tile.cpp
    Tile.h
    TileRender.cpp
    TileRender.h
    Tileset.cpp
    Tileset.h
    Vector.cpp
    Vector.h
    Window.cpp
    Window.h
    Window_SDL1.cpp
    Window_SDL2.cpp
)

if(APPLE AND NOT AQUARIA_USE_SDL2)
    set(BBGE_SRCS ${BBGE_SRCS} Cocoa.mm)
endif()

add_library(BBGE ${BBGE_SRCS})
target_link_libraries(BBGE ExternalLibs ${SDL_LIBRARY})
