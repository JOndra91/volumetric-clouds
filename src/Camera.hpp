#pragma once

#include <SDL.h>
#include <glm/glm.hpp>

#include "IEventListener.hpp"
#include "IProcessor.hpp"
#include "IRenderer.hpp"

namespace pgp {

    using namespace glm;

    class Camera : public IEventListener, public IProcessor, public IRenderer {
    protected:
        SDL_Window *window;
        ivec2 windowSize;
        vec3 position;
        vec2 rotation;
        ivec3 movement;

    public:
        Camera(SDL_Window *window);

        inline vec3 getPosition() {
            return position;
        };

        inline ivec2 getWindowSize() {
            return windowSize;
        }

        vec3 getViewVector();

        virtual IEventListener::EventResponse onEvent(SDL_Event* evt);

        virtual void step(float time, float delta);

        virtual void render();

    };

}
