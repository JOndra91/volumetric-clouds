#include <climits>
#include <glm/glm.hpp>
#include <string>
#include <glm/gtc/matrix_transform.hpp>

#include "Landscape.hpp"

#define LANDSCAPE_SIZE 350
#define LANDSCAPE_SIZEF float(LANDSCAPE_SIZE)
#define INDEX_COUNT (2 * (LANDSCAPE_SIZE + 1) * LANDSCAPE_SIZE + LANDSCAPE_SIZE)
#define BASE_FREQUENCY 100
#define RESOLUTION 0.85

using namespace pgp;

using glm::u8vec3;
using glm::vec3;

typedef struct {
    vec3 position;
    vec3 normal;
    u8vec3 color;
} Vertex;

Landscape::Landscape(Camera *_camera) : camera(_camera), vao(0), vbo(0), ebo(0), polygonMode(GL_FILL) {
    string vertexShaderFile("./shaders/landscape.vert");
    string fragmentShaderFile("./shaders/landscape.frag");

    registerRenderer(camera);

    renderProgram.setVertexShaderFromFile(vertexShaderFile);
    renderProgram.setFragmenShaderFromFile(fragmentShaderFile);

    GLuint program = renderProgram.getProgram();
    if (program == 0) {
        throw string("Could not compile render program.");
    }

    uProjection = glGetUniformLocation(program, "projection");
    uView = glGetUniformLocation(program, "view");
    uEyePosition = glGetUniformLocation(program, "eyePosition");
    uSunPosition = glGetUniformLocation(program, "sunPosition");
    uSunColor = glGetUniformLocation(program, "sunColor");

    aPosition = glGetAttribLocation(program, "position");
    aNormal = glGetAttribLocation(program, "normal");
    aColor = glGetAttribLocation(program, "color");

    ivec2 windowSize = camera->getWindowSize();

    glGenTextures(1, &colTex);
    glGenTextures(1, &depTex);

    glBindTexture(GL_TEXTURE_2D, colTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, windowSize.x, windowSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, depTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, windowSize.x, windowSize.y, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, windowSize.x, windowSize.y);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, depTex, 0);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBuffers);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (LANDSCAPE_SIZE + 1) * (LANDSCAPE_SIZE + 1) * sizeof (Vertex), NULL, GL_STREAM_DRAW);

    glEnableVertexAttribArray(aPosition);
    glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, sizeof (Vertex), (GLvoid*) offsetof(Vertex, position));

    glEnableVertexAttribArray(aNormal);
    glVertexAttribPointer(aNormal, 3, GL_FLOAT, GL_FALSE, sizeof (Vertex), (GLvoid*) offsetof(Vertex, normal));

    glEnableVertexAttribArray(aColor);
    glVertexAttribPointer(aColor, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof (Vertex), (GLvoid*) offsetof(Vertex, color));

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, INDEX_COUNT * sizeof (GLuint), NULL, GL_STATIC_DRAW);

    GLuint *eboData = (GLuint*) glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);

    GLuint *dataPtr = eboData;
    //    int ctr = 0;
    for (int row = 0; row < LANDSCAPE_SIZE; row++) {
        for (int col = 0; col <= LANDSCAPE_SIZE; col++) {
            int i = (row + 1) * (LANDSCAPE_SIZE + 1) + col;
            int j = row * (LANDSCAPE_SIZE + 1) + col;

            dataPtr[0] = i;
            dataPtr[1] = j;
            dataPtr += 2;
            //            ctr += 2;

            //            std::cout << "Element: " << i << std::endl;
            //            std::cout << "Element: " << j << std::endl;
        }

        dataPtr[0] = UINT_MAX; // Reset index
        dataPtr++;
        //        ctr++;

        //        std::cout << "Element: " << USHRT_MAX << std::endl;

    }

    //    std::cout << "Size: " << ctr << std::endl;
    //    std::cout << "Estimated size: " << INDEX_COUNT << std::endl;

    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

    glBindVertexArray(0);

    center = camera->getPosition();

    heightmap = new float[(LANDSCAPE_SIZE + 2) * (LANDSCAPE_SIZE + 2)];

    reloadTerrain();
}

Landscape::~Landscape() {
    glDeleteFramebuffers(1, &fbo);

    glDeleteRenderbuffers(1, &rbo);

    glDeleteTextures(1, &colTex);
    glDeleteTextures(1, &depTex);

    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);

    glDeleteVertexArrays(1, &vao);

    delete heightmap;
}


#ifdef WRITE_HEIGHTMAP

void writePGM(int width, int height, unsigned char *data, std::string filename) {

    std::ofstream file(filename.c_str());

    file << "P5" << std::endl;
    file << width << " " << height << std::endl;
    file << "255" << std::endl;

    file.write((char*) data, width * height * sizeof (unsigned char));

    file.close();
}
#endif

void Landscape::reloadTerrain() {

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    Vertex *vboData = (Vertex*) glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

    vec3 pos = camera->getPosition();
    float *map = heightmap;

    float max = 0;
    for (int row = -1; row <= LANDSCAPE_SIZE; row++) {
        for (int col = -1; col <= LANDSCAPE_SIZE; col++) {
            float x, y, z;

            x = ((row - 1) - (LANDSCAPE_SIZEF / 2.0)) * RESOLUTION + pos.x;
            z = ((col - 1) - (LANDSCAPE_SIZEF / 2.0)) * RESOLUTION + pos.z;
            y = 0;

            y += parametrizedNoise(x, z, 4.0 / BASE_FREQUENCY, 2.0 / BASE_FREQUENCY, 65.0);
            y += parametrizedNoise(x + 7769.0, z + 1103.0, 16.0 / BASE_FREQUENCY, 18.0 / BASE_FREQUENCY, 5.0);
            y += parametrizedNoise(x - 356.0, z + 32776.0, 64.0 / BASE_FREQUENCY, 64.0 / BASE_FREQUENCY, 0.5);

            *map = y;
            map++;

            if (y > max) {
                max = y;
            }
        }
    }

#ifdef WRITE_HEIGHTMAP
    unsigned char *_out = new unsigned char[(LANDSCAPE_SIZE + 2) * (LANDSCAPE_SIZE + 2)];
    unsigned char *out = _out;

    map = heightmap;

    for (int row = -1; row <= LANDSCAPE_SIZE; row++) {
        for (int col = -1; col <= LANDSCAPE_SIZE; col++) {
            int i = (row + 1) * (LANDSCAPE_SIZE + 2) + col;
            *out = heightmap[i]*255 / max;
            out++;
        }
    }

    writePGM((LANDSCAPE_SIZE + 2), (LANDSCAPE_SIZE + 2), _out, "heightmap.pgm");

    delete _out;
#endif

    Vertex *dataPtr = vboData;
    for (int row = 0; row <= LANDSCAPE_SIZE; row++) {
        for (int col = 0; col <= LANDSCAPE_SIZE; col++) {
            Vertex *v = dataPtr++;
            vec3 a, b, c, d, g, h;

            a.x = ((row) - (LANDSCAPE_SIZEF / 2)) * RESOLUTION + pos.x;
            a.y = heightmap[(row) * (LANDSCAPE_SIZE + 2) + (col)];
            a.z = ((col) - (LANDSCAPE_SIZEF / 2)) * RESOLUTION + pos.z;

            b.x = ((row + 1) - (LANDSCAPE_SIZEF / 2)) * RESOLUTION + pos.x;
            b.y = heightmap[(row + 1) * (LANDSCAPE_SIZE + 2) + (col)];
            b.z = ((col) - (LANDSCAPE_SIZEF / 2)) * RESOLUTION + pos.z;

            c.x = ((row + 1) - (LANDSCAPE_SIZEF / 2)) * RESOLUTION + pos.x;
            c.y = heightmap[(row + 1) * (LANDSCAPE_SIZE + 2) + (col + 1)];
            c.z = ((col + 1) - (LANDSCAPE_SIZEF / 2)) * RESOLUTION + pos.z;

            d.x = ((row) - (LANDSCAPE_SIZEF / 2)) * RESOLUTION + pos.x;
            d.y = heightmap[(row) * (LANDSCAPE_SIZE + 2) + (col + 1)];
            d.z = ((col + 1) - (LANDSCAPE_SIZEF / 2)) * RESOLUTION + pos.z;

            g = (a + b) * 0.5f;
            h = (c + d) * 0.5f;
            v->position = (g + h) * 0.5f;

            a = normalize(a - c);
            b = normalize(b - d);

            v->normal = normalize(cross(b, a));

            //            std::cout << "Normal: (" << v->normal.x << ", " << v->normal.y << ", " << v->normal.z << ")" << std::endl;

            // Calculate color
            v->color = u8vec3(0, 80, 0); // Color is green, grass is green, what is nice color it is. (Haiku?)

        }
    }

    glUnmapBuffer(GL_ARRAY_BUFFER);
}

float Landscape::smoothNoise2D(float _x, float _y) {
    int x0 = floor(_x);
    int y0 = floor(_y);

    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float rx = _x - x0;
    float ry = _y - y0;

    float a0 = 0, a1 = 0, b0 = 0, b1 = 0;

    a0 = noise2D(x0, y0);
    a1 = noise2D(x1, y0);

    b0 = noise2D(x0, y1);
    b1 = noise2D(x1, y1);

    float a = interpolateCos(a0, a1, rx);
    float b = interpolateCos(b0, b1, rx);

    return interpolateCos(a, b, ry);

}

float Landscape::noise2D(int x, int y) {
    x += 338573;
    y += 77313501;
    unsigned int n = ((((x * x) << 3) * 23) + ((y * y) << 1) * 51);

    return (((((n * 3342687 + 1144763) & 0xf2fcf7dd) - 77663544) * -113) * n) / (float) UINT_MAX;
}

float Landscape::interpolateCos(float a, float b, float factor) {
    factor = (1 - cos(factor * glm::pi<float>())) * 0.5;

    return a * (1 - factor) + b * factor;
}

mat4 Landscape::getProjectionMatrix() {

      vec2 windowSize = camera->getWindowSize();
      float fov = radians(75.0);
      float aspect = windowSize.x / windowSize.y;
      float near = 0.001;
      float far = 1e5;

      return glm::perspective(fov, aspect, near, far);
}

mat4 Landscape::getViewMatrix() {

      vec3 cameraPosition = camera->getPosition();
      vec3 atPosition = cameraPosition + camera->getViewVector();

      return glm::lookAt(cameraPosition, atPosition, vec3(0, 1, 0));
}

void Landscape::render() {

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    runRenderers();

    glUseProgram(renderProgram.getProgram());

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glClearColor(0.0, 0.7, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float depClear = 1e15;
    glClearTexImage(depTex, 0, GL_RED, GL_FLOAT, &depClear);

    mat4 viewMat = getViewMatrix();
    mat4 projMat = getProjectionMatrix();

    vec3 camPos = camera->getPosition();

    // std::cout << "Eye: (" << camPos.x << ", " << camPos.y << ", " << camPos.z << ")" << std::endl;
    // std::cout << "At: (" << atPosition.x << ", " << atPosition.y << ", " << atPosition.z << ")" << std::endl;

    glUniformMatrix4fv(uView, 1, GL_FALSE, (GLfloat*) & viewMat);
    glUniformMatrix4fv(uProjection, 1, GL_FALSE, (GLfloat*) & projMat);

    glUniform3fv(uEyePosition, 1, &camPos[0]);

    glBindVertexArray(vao);

    glPolygonMode(GL_FRONT_AND_BACK, polygonMode);

    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glDrawElements(GL_TRIANGLE_STRIP, INDEX_COUNT, GL_UNSIGNED_INT, 0);
    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Landscape::step(float time, float delta) {
    if (distance(center, camera->getPosition()) > 15.0) {
        reloadTerrain();
        center = camera->getPosition();
    }
}

IEventListener::EventResponse Landscape::onEvent(SDL_Event* evt) {

    if (evt->type == SDL_WINDOWEVENT) {
        SDL_WindowEvent *e = &evt->window;

        if (e->event == SDL_WINDOWEVENT_RESIZED
                || e->event == SDL_WINDOWEVENT_SIZE_CHANGED) {

            ivec2 windowSize = camera->getWindowSize();

            glBindTexture(GL_TEXTURE_2D, colTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, windowSize.x, windowSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

            glBindTexture(GL_TEXTURE_2D, depTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, windowSize.x, windowSize.y, 0, GL_RED, GL_FLOAT, NULL);

            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, windowSize.x, windowSize.y);

            glBindTexture(GL_TEXTURE_2D, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            return EVT_PROCESSED;
        }
    } else if (evt->type == SDL_KEYDOWN) {
        SDL_KeyboardEvent *e = &evt->key;

        if (e->keysym.sym == SDLK_p) {
            switch (polygonMode) {
                case GL_FILL:
                    polygonMode = GL_LINE;
                    break;
                case GL_LINE:
                    polygonMode = GL_POINT;
                    break;
                case GL_POINT:
                    polygonMode = GL_FILL;
                    break;
            }
            return EVT_PROCESSED;
        }
    }

    return EVT_IGNORED;
}
