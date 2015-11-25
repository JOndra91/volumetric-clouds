#pragma once

#define S_OK 0
#define S_SDL_ERROR 1

#include <SDL.h>

#include "RegistrablesContainer.hpp"
#include "Camera.hpp"
#include "Landscape.hpp"
#include "Clouds.hpp"

namespace pgp {

    class Main : public RegistrablesContainer, public IEventListener {
    protected:
        SDL_Window *sdlWindow;
        SDL_GLContext context;
        bool quitFlag = false;
        Camera *camera;
        Landscape *landscape;
        Clouds *clouds;

    public:
        Main();
        ~Main();
        void run();
        void init();
        void onQuit();

        inline void quit() {
            quitFlag = true;
        };

        // Event listener interface
        virtual IEventListener::EventResponse onEvent(SDL_Event* evt);

    };
}

int main(int argc, char **argv);
