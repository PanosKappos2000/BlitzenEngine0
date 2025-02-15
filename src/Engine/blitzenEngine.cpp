/* 
    The code in this file calls all of the functions needed to run the applications.
    Contains the main function at the bottom
*/
 
#include "Engine/blitzenEngine.h"
#include "Platform/platform.h"
#include "Renderer/blitRenderer.h"
#include "Core/blitzenCore.h"
#include "Game/blitCamera.h"

#ifdef BLITZEN_VULKAN
    #define BLIT_ACTIVE_RENDERER_ON_BOOT      BlitzenEngine::ActiveRenderer::Vulkan
#elif BLITZEN_GL && _MSC_VER_
    #define BLIT_ACTIVE_RENDERER_ON_BOOT      BlitzenEngine::ActiveRenderer::OpenGL
#else
    #define BLIT_ACTIVE_RENDERER_ON_BOOT      BlitzenEngine::ActiveRenderer::MaxRenderers
#endif

namespace BlitzenEngine
{
    // Static member variable needs to be declared in the .cpp file as well
    Engine* Engine::s_pEngine;

    Engine::Engine()
    {
        // There should not be a 2nd instance of Blitzen Engine
        if(GetEngineInstancePointer())
        {
            BLIT_ERROR("Blitzen is already active")
            return;
        }

        // Initialize the engine if it is the first time the constructor is called
        else
        {
            // Initalize the instance and the system boolean to avoid creating or destroying a 2nd instance
            s_pEngine = this;
            BLIT_INFO("%s Booting", BLITZEN_VERSION)
        }
    }



    /*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        This function currently holds the majority of the functionality called during runtime.
        Its scope owns the memory used by each renderer(only Vulkan for the forseeable future.
        It calls every function needed to draw a frame and other functionality the engine has
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
    void Engine::Run(uint32_t argc, char* argv[])
    {
        // Initialize logging
        BlitzenCore::InitLogging();

        // Initialize the camera stystem
        BlitzenEngine::CameraSystem cameraSystem;

        // Initialize the event system as a smart pointer
        BlitCL::SmartPointer<BlitzenCore::EventSystemState> eventSystemState;

        // Initialize the input system after the event system
        BlitCL::SmartPointer<BlitzenCore::InputSystemState> inputSystemState;

        // Platform specific code initalization. 
        // This should be called after the event system has been initialized because the event function is called.
        // That will break the application without the event system.
        BLIT_ASSERT(BlitzenPlatform::PlatformStartup(BLITZEN_VERSION, BLITZEN_WINDOW_STARTING_X, 
        BLITZEN_WINDOW_STARTING_Y, BLITZEN_WINDOW_WIDTH, BLITZEN_WINDOW_HEIGHT))
            
        // With the event and input systems active, register the engine's default events and input bindings
        RegisterDefaultEvents();

        // Rendering system holds all API specific renderer (Vulkan and Opengl for now)
        BlitCL::SmartPointer<RenderingSystem, BlitzenCore::AllocationType::Renderer> renderer;
        
        // Allocated the rendering resources on the heap, it is too big for the stack of this function
        BlitCL::SmartPointer<BlitzenEngine::RenderingResources, BlitzenCore::AllocationType::Renderer> pResources;
        BLIT_ASSERT_MESSAGE(BlitzenEngine::LoadRenderingResourceSystem(pResources.Data()), "Failed to acquire resource system")
        
        // If the engine passes the above assertion, then it means that it can run the main loop (unless some less fundamental stuff makes it fail)
        isRunning = 1;
        isSupended = 0;

        // Setup the main camera
        Camera& mainCamera = cameraSystem.GetCamera();
        SetupCamera(mainCamera, BLITZEN_FOV, static_cast<float>(BLITZEN_WINDOW_WIDTH), 
        static_cast<float>(BLITZEN_WINDOW_HEIGHT), BLITZEN_ZNEAR, BlitML::vec3(30.f, 100.f, 0.f), BLITZEN_DRAW_DISTANCE);

        // Loads obj meshes that will be draw with "random" transforms by the millions to stress the renderer
        #ifdef BLITZEN_RENDERING_STRESS_TEST
            uint32_t drawCount = 4'500'000;// Rendering a large amount of objects to stress test the renderer
            LoadGeometryStressTest(pResources.Data(), drawCount, renderer->IsVulkanAvailable(), renderer->IsOpenglAvailable());
        #else
            uint32_t drawCount = 999;
            LoadGeometryStressTest(pResources.Data(), drawCount, renderer->IsVulkanAvailable(), renderer->IsOpenglAvailable());
        #endif


        // Loads the gltf files that were specified as command line arguments
        if(argc != 1)
        {
            for(uint32_t i = 1; i < argc; ++i)
            {
                LoadGltfScene(pResources.Data(), argv[i], renderer->IsVulkanAvailable(), renderer->IsOpenglAvailable());
            }
        }

        // Set the draw count to the render object count   
        drawCount = pResources.Data()->renderObjectCount;

        // Pass the resources and pointers to any of the renderers that might be used for rendering
        BLIT_ASSERT(renderer->SetupRequestedRenderersForDrawing(pResources.Data(), drawCount, mainCamera));/* I use an assertion here
        but it could be handled some other way as well */
        
        // Start the clock
        m_clockStartTime = BlitzenPlatform::PlatformGetAbsoluteTime();
        m_clockElapsedTime = 0;
        double previousTime = m_clockElapsedTime;// Initialize previous frame time to the elapsed time

        // Main Loop starts
        while(isRunning)
        {
            // Always returns 1 (not sure if I want messages to stop the application ever)
            if(!BlitzenPlatform::PlatformPumpMessages())
            {
                isRunning = 0;
            }

            if(!isSupended)
            {
                // Get the elapsed time of the application
                m_clockElapsedTime = BlitzenPlatform::PlatformGetAbsoluteTime() - m_clockStartTime;
                // Update the delta time by using the previous elapsed time
                m_deltaTime = m_clockElapsedTime - previousTime;
                // Update the previous elapsed time to the current elapsed time
                previousTime = m_clockElapsedTime;

                // With delta time retrieved, call update camera to make any necessary changes to the scene based on its transform
                UpdateCamera(mainCamera, (float)m_deltaTime);

                // Draw the frame!!!!
                renderer->DrawFrame(mainCamera, drawCount);

                // Make sure that the window resize is set to false after the renderer is notified
                mainCamera.transformData.windowResize = 0;

                BlitzenCore::UpdateInput(m_deltaTime);
            }
        }

        // Shutdown the renderers before the engine is shutdown
        renderer->ShutdownRenderers();

        // With the main loop done, Blitzen calls Shutdown on itself
        Shutdown();
    }

    void Engine::Shutdown()
    {
        if (s_pEngine)
        {
            BLIT_WARN("Blitzen is shutting down")

            BlitzenCore::ShutdownLogging();

            BlitzenPlatform::PlatformShutdown();

            s_pEngine = nullptr;
        }

        // The destructor should not be called more than once as it will crush the application
        else
        {
            BLIT_ERROR("Any uninitialized instances of Blitzen will not be explicitly cleaned up")
        }
    }

    void Engine::UpdateWindowSize(uint32_t newWidth, uint32_t newHeight) 
    {
        Camera& camera = CameraSystem::GetCameraSystem()->GetCamera();
        camera.transformData.windowResize = 1;
        if(newWidth == 0 || newHeight == 0)
        {
            isSupended = 1;
            return;
        }
        isSupended = 0;
        UpdateProjection(camera, BLITZEN_FOV, static_cast<float>(newWidth), static_cast<float>(newHeight), BLITZEN_ZNEAR);
    }
}







int main(int argc, char* argv[])
{
    // Memory management is initialized here. The engine destructor must be called before the memory manager destructor
    BlitzenCore::MemoryManagerState blitzenMemory;

    // Blitzen engine lives in this scope, it needs to go out of scope before memory management shuts down
    {
        // I could have the Engine be stack allocated, but I am keeping it as a smart pointer for now
        BlitCL::SmartPointer<BlitzenEngine::Engine, BlitzenCore::AllocationType::Engine> engine;

        engine.Data()->Run(argc, argv);
    }
}
//Assets/Scenes/CityLow/scene.gltf ../../GltfTestScenes/Scenes/Plaza/scene.gltf ../../GltfTestScenes/Scenes/Museum/scene.gltf (personal test scenes for copy+paste)