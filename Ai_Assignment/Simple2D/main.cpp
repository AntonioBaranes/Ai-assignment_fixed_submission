/**
* Author: [Antonio Baranes]
* Assignment: Lunar Lander
* Date due: 2024-10-27, 11:59pm
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/

#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION
#define LOG(argument) std::cout << argument << '\n'
#define GL_GLEXT_PROTOTYPES 1
#define FIXED_TIMESTEP 0.0166666f
#define PLATFORM_COUNT 3

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_mixer.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include <random>
#include <cstdlib>
#include<string>
#include "Entity.h"
#include<chrono>
#include <thread>



// ––––– STRUCTS AND ENUMS ––––– //
struct GameState
{
    Entity* player;
    Entity* platforms;
    Entity* enemies;
    Entity* attack;
};

enum moment {LAND, WON, LOST};
moment state;

// ––––– CONSTANTS ––––– //
constexpr int WINDOW_WIDTH = 640,
WINDOW_HEIGHT = 480;

constexpr float BG_RED = 0.1922f,
BG_BLUE = 0.549f,
BG_GREEN = 0.9059f,
BG_OPACITY = 1.0f;

constexpr int VIEWPORT_X = 0,
VIEWPORT_Y = 0,
VIEWPORT_WIDTH = WINDOW_WIDTH,
VIEWPORT_HEIGHT = WINDOW_HEIGHT;

constexpr char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

constexpr float MILLISECONDS_IN_SECOND = 1000.0;
constexpr char SPRITESHEET_FILEPATH[] = "assets/spr_yamcha.png";
constexpr char ENEMY_FILEPATH[] = "assets/spr_saibaman.png";
constexpr char PLATFORM_FILEPATH[] = "assets/spr_platform.png";
constexpr char FONTSHEET_FILEPATH[] = "assets/font1.png";
constexpr char SOKIDAN_FILEPATH[] = "assets/spr_sokidan.png";

constexpr int NUMBER_OF_TEXTURES = 1;
constexpr GLint LEVEL_OF_DETAIL = 0;
constexpr GLint TEXTURE_BORDER = 0;

constexpr int CD_QUAL_FREQ = 44100,
AUDIO_CHAN_AMT = 2,     // stereo
AUDIO_BUFF_SIZE = 4096;

constexpr char BGM_FILEPATH[] = "assets/snd_bgm.mp3",
SFX_FILEPATH[] = "assets/snd_dash.wav";

constexpr int PLAY_ONCE = 0,    // play once, loop never
NEXT_CHNL = -1,   // next available channel
ALL_SFX_CHNL = -1;

float ANGLE = 0.0F;

Mix_Music* g_music;
Mix_Chunk* g_jump_sfx;

// ––––– GLOBAL VARIABLES ––––– //
GameState g_state;

SDL_Window* g_display_window;
bool g_game_is_running = true;

ShaderProgram g_shader_program = ShaderProgram();
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f;
float g_accumulator = 0.0f;

glm::vec3 plat_pos;

GLuint g_font_texture_id;

bool pause = false;

int g_enemy_count = 3;
int g_score = 0;
glm::vec3 g_player_position = glm::vec3(0.0f, 3.5f, 0.0f);

constexpr glm::vec3 init_player_position = glm::vec3(0.0f, 3.5f, 0.0f);

void delay(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// ––––– GENERAL FUNCTIONS ––––– //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);



    stbi_image_free(image);

    return textureID;
}

constexpr int FONTBANK_SIZE = 16;

void draw_text(ShaderProgram* program, GLuint font_texture_id, std::string text,
    float font_size, float spacing, glm::vec3 position)
{
    // Scale the size of the fontbank in the UV-plane
    // We will use this for spacing and positioning
    float width = 1.0f / FONTBANK_SIZE;
    float height = 1.0f / FONTBANK_SIZE;

    // Instead of having a single pair of arrays, we'll have a series of pairs—one for
    // each character. Don't forget to include <vector>!
    std::vector<float> vertices;
    std::vector<float> texture_coordinates;

    // For every character...
    for (int i = 0; i < text.size(); i++) {
        // 1. Get their index in the spritesheet, as well as their offset (i.e. their
        //    position relative to the whole sentence)
        int spritesheet_index = (int)text[i];  // ascii value of character
        float offset = (font_size + spacing) * i;

        // 2. Using the spritesheet index, we can calculate our U- and V-coordinates
        float u_coordinate = (float)(spritesheet_index % FONTBANK_SIZE) / FONTBANK_SIZE;
        float v_coordinate = (float)(spritesheet_index / FONTBANK_SIZE) / FONTBANK_SIZE;

        // 3. Inset the current pair in both vectors
        vertices.insert(vertices.end(), {
            offset + (-0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
            });

        texture_coordinates.insert(texture_coordinates.end(), {
            u_coordinate, v_coordinate,
            u_coordinate, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate + width, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate, v_coordinate + height,
            });
    }

    // 4. And render all of them using the pairs
    glm::mat4 model_matrix = glm::mat4(1.0f);
    model_matrix = glm::translate(model_matrix, position);

    program->set_model_matrix(model_matrix);
    glUseProgram(program->get_program_id());

    glVertexAttribPointer(program->get_position_attribute(), 2, GL_FLOAT, false, 0,
        vertices.data());
    glEnableVertexAttribArray(program->get_position_attribute());
    glVertexAttribPointer(program->get_tex_coordinate_attribute(), 2, GL_FLOAT, false, 0,
        texture_coordinates.data());
    glEnableVertexAttribArray(program->get_tex_coordinate_attribute());

    glBindTexture(GL_TEXTURE_2D, font_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, (int)(text.size() * 6));

    glDisableVertexAttribArray(program->get_position_attribute());
    glDisableVertexAttribArray(program->get_tex_coordinate_attribute());
}

void won() {}

void lost() {}

void initialize_window() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    g_display_window = SDL_CreateWindow("Yamcha's Revenge!",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);

#ifdef _WINDOWS
    glewInit();
#endif

    // ––––– VIDEO ––––– //
    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);

    glUseProgram(g_shader_program.get_program_id());

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);

    // ––––– BGM ––––– //
    Mix_OpenAudio(CD_QUAL_FREQ, MIX_DEFAULT_FORMAT, AUDIO_CHAN_AMT, AUDIO_BUFF_SIZE);

    // STEP 1: Have openGL generate a pointer to your music file
    g_music = Mix_LoadMUS(BGM_FILEPATH); // works only with mp3 files

    // STEP 2: Play music
    Mix_PlayMusic(
        g_music,  // music file
        -1        // -1 means loop forever; 0 means play once, look never
    );

    // STEP 3: Set initial volume
    Mix_VolumeMusic(MIX_MAX_VOLUME / 2.0);

    // ––––– SFX ––––– //
    g_jump_sfx = Mix_LoadWAV(SFX_FILEPATH);

    GLuint player_texture_id = load_texture(SPRITESHEET_FILEPATH);
    /*
    int player_walking_animation[4][4] =
    {
        { 1, 5, 9, 13 },  // for George to move to the left,
        { 3, 7, 11, 15 }, // for George to move to the right,
        { 2, 6, 10, 14 }, // for George to move upwards,
        { 0, 4, 8, 12 }   // for George to move downwards
    };
    */
    //int player_walking_animation[1][1]{ {1} };
    //glm::vec3 acceleration = glm::vec3(0.0f, -18.905f, 0.0f);
    
    glm::vec3 acceleration = glm::vec3(0.0f, -18.905f, 0.0f);


    g_state.player = new Entity;
    glm::vec3 player_scaler = glm::vec3(0.7f, 1.0f, 0.0f);
    //g_state.player->set_scale(player_scaler);
    g_state.player->set_texture_id(player_texture_id);
    g_state.player->set_speed(0.1f);
    g_state.player->set_acceleration(acceleration);
    //g_state.player->set_jumping_power(3.0f);
    g_state.player->set_width(1.0f);
    g_state.player->set_height(1.0f);
    g_state.player->set_entity_type(PLAYER);
    g_state.player->set_position(g_player_position);

    GLuint attack_texture_id = load_texture(SOKIDAN_FILEPATH);


    //Sokidan!
    g_state.attack = new Entity;
    //glm::vec3 player_scaler = glm::vec3(0.7f, 1.0f, 0.0f);
    //g_state.player->set_scale(player_scaler);
    g_state.attack->set_texture_id(attack_texture_id);
    g_state.attack->set_speed(0.1f);
    g_state.attack->set_acceleration(glm::vec3(0.0f, -5.905, 0.0f));
    //g_state.player->set_jumping_power(3.0f);
    g_state.attack->set_width(1.0f);
    g_state.attack->set_height(1.0f);
    g_state.attack->set_entity_type(ATTACK);
    g_state.attack->set_position(glm::vec3(0.0f, 2.0f, 0.0f));

    
}

void initialise()
{

    // ––––– PLATFORMS ––––– //
    GLuint platform_texture_id = load_texture(PLATFORM_FILEPATH);

    g_state.platforms = new Entity[3];
    /*
    std::random_device rd;        
    std::mt19937 gen(rd());        
    std::uniform_real_distribution<float> x_dist(-5.0f, 5.0f); // X position range for random spawn

    float random_x = x_dist(gen);
    */

    glm::vec3 scaler_platform = glm::vec3(2.2f, 1.0f, 0.0f);

    float platform_width = 0.8f;

    g_state.platforms[0].set_width(platform_width);
    g_state.platforms[0].set_height(0.3f);
    g_state.platforms[0].set_entity_type(PLATFORM);
    g_state.platforms[0].set_texture_id(platform_texture_id);
    //g_state.platforms[0].set_scale(scaler_platform);
    g_state.platforms[0].set_position(glm::vec3(-3.5f, -1.0f, 0.0f));
    
    g_state.platforms[1].set_width(platform_width);
    g_state.platforms[1].set_height(0.3f);
    g_state.platforms[1].set_entity_type(PLATFORM);
    g_state.platforms[1].set_texture_id(platform_texture_id);
    //g_state.platforms[1].set_scale(scaler_platform);
    g_state.platforms[1].set_position(glm::vec3(0.0f, -3.0f, 0.0f));
    
    g_state.platforms[2].set_width(platform_width);
    g_state.platforms[2].set_height(0.3f);
    g_state.platforms[2].set_entity_type(PLATFORM);
    g_state.platforms[2].set_texture_id(platform_texture_id);
    //g_state.platforms[2].set_scale(scaler_platform);
    g_state.platforms[2].set_position(glm::vec3(3.5f, -1.0f, 0.0f));
    

    g_state.platforms[0].update(0.0f, nullptr, nullptr, 0);
    g_state.platforms[1].update(0.0f, nullptr, nullptr, 0);
    g_state.platforms[2].update(0.0f, nullptr, nullptr, 0);

    //Enemies
    GLuint enemy_texture_id = load_texture(ENEMY_FILEPATH);

    g_state.enemies = new Entity[3];


    glm::vec3 scaler_enemies = glm::vec3(0.7f, 1.0f, 0.0f);

    g_state.enemies[0].set_texture_id(enemy_texture_id);
    g_state.enemies[0].set_position(glm::vec3(-3.5f, -0.3f, 0.0f));
    g_state.enemies[0].set_width(1.0f);
    g_state.enemies[0].set_height(1.0f);
    g_state.enemies[0].set_speed(0.025f);
    g_state.enemies[0].set_entity_type(ENEMY_ROTATOR);
   // g_state.enemies[0].set_scale(scaler_enemies);
    
    g_state.enemies[1].set_texture_id(enemy_texture_id);
    g_state.enemies[1].set_position(glm::vec3(0.0f, -1.5f, 0.0f));
    g_state.enemies[1].set_width(1.0f);
    g_state.enemies[1].set_height(1.0f);
    g_state.enemies[1].set_jumping_power(30.0f);
    g_state.enemies[1].set_speed(1.4f);
    g_state.enemies[1].set_velocity(glm::vec3(0.0f, 0.0f, 0.0f));
    g_state.enemies[1].set_acceleration(glm::vec3(0.0f, -20.0f, 0.0f));
    g_state.enemies[1].set_entity_type(ENEMY_JUMPER);
    //g_state.enemies[1].set_scale(scaler_enemies);

    g_state.enemies[2].set_texture_id(enemy_texture_id);
    g_state.enemies[2].set_position(glm::vec3(3.5f, -0.3f, 0.0f));
    g_state.enemies[2].set_width(1.0f);
    g_state.enemies[2].set_height(1.0f);
    g_state.enemies[2].set_speed(0.6f);
    g_state.enemies[2].set_entity_type(ENEMY_FOLLOWER);
    //g_state.enemies[2].set_scale(scaler_enemies);
    
    g_state.enemies[0].update(0.0f, nullptr, nullptr, 0);
    g_state.enemies[1].update(0.0f, nullptr, nullptr, 0);
    g_state.enemies[2].update(0.0f, nullptr, nullptr, 0);


    glm::vec3 acceleration = glm::vec3(0.0f, -18.905f, 0.0f);

    

    // ––––– PLAYER (GEORGE) ––––– //
    


    
    /*
    g_state.player = new Entity(
        player_texture_id,         // texture id
        0.1f,                      // speed
        acceleration,              // acceleration
        3.0f,                      // jumping power
        player_walking_animation,  // animation index sets
        0.0f,                      // animation time
        4,                         // animation frame amount
        0,                         // current animation index
        4,                         // animation column amount
        4,                         // animation row amount
        1.0f,                      // width
        1.0f,                       // height
        PLAYER
    );
    */

    // Jumping
    //g_state.player->set_jumping_power(3.0f);
    //g_state.player->set_acceleration(acceleration);
    //g_state.player->set_position(glm::vec3(0.0, 2.0, 0.0));
    
    g_font_texture_id = load_texture(FONTSHEET_FILEPATH);

    // ––––– GENERAL ––––– //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);



}

void initalize_objects() {

}

void process_input()
{
    g_state.player->set_movement(glm::vec3(0.0f));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            // End game
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_game_is_running = false;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_game_is_running = false;
                break;

            case SDLK_SPACE:
                // Jump
                if (g_state.player->get_collided_bottom())
                {
                    //g_state.player->jump();
                    Mix_PlayChannel(NEXT_CHNL, g_jump_sfx, 0);
                }
                break;

            case SDLK_h:
                // Stop music
                Mix_HaltMusic();
                break;

            case SDLK_p:
                Mix_PlayMusic(g_music, -1);

            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);

    if (key_state[SDL_SCANCODE_RIGHT]) {
        g_state.attack->move_right();
    }
    else if (key_state[SDL_SCANCODE_LEFT]) {
        g_state.attack->move_left();
    }
    if (key_state[SDL_SCANCODE_UP]) {
        g_state.attack->set_movement(glm::vec3(0.0f, 0.01f, 0.0f));
    }else if (key_state[SDL_SCANCODE_DOWN]) {
        g_state.attack->move_down();
    }

    if (key_state[SDL_SCANCODE_A])
    {
        g_state.player->rot_left();
        std::cout << "rotating Left\n";
    }
    else if (key_state[SDL_SCANCODE_D])
    {
        g_state.player->rot_right();
        
    }
    if (key_state[SDL_SCANCODE_SPACE]) {
        g_state.player->jump();
        if (g_state.player->get_speed() < 1) {
            g_state.player->set_speed(1.0f);
        }
        else {
            g_state.player->incr_speed();
        }
    }
    else {
        g_state.player->decr_speed();
    }

    if (glm::length(g_state.player->get_movement()) > 1.0f)
    {
        g_state.player->normalise_movement();
    }
    if (glm::length(g_state.attack->get_movement()) > 1.0f)
    {
        g_state.attack->normalise_movement();
    }
}


void render()
{
    glClear(GL_COLOR_BUFFER_BIT);

    g_state.player->render(&g_shader_program);
    g_state.attack->render(&g_shader_program);

    for (int i = 0; i < PLATFORM_COUNT; i++) {
        if (g_state.platforms[i].get_entity_type() == EntityType::PLATFORM)
            g_state.platforms[i].render(&g_shader_program);
    }
    for (int i = 0; i < 3; i++) {
        g_state.enemies[i].render(&g_shader_program);
    }
    std::string str_score = "Score: " + std::to_string(g_score);
    draw_text(&g_shader_program, g_font_texture_id, str_score , 0.5f, 0.05f, (glm::vec3(-4.0f, 3.0f, 0.0f)));
    if (state == moment::LAND) {
        draw_text(&g_shader_program, g_font_texture_id, "Stop all The ", 0.5f, 0.05f,
            glm::vec3(-3.5f, 2.0f, 0.0f));
        draw_text(&g_shader_program, g_font_texture_id, "Saibamen!", 0.5f, 0.05f,
            glm::vec3(-3.5f, 1.0f, 0.0f));
    }
    else if (state == moment::WON) {
        draw_text(&g_shader_program, g_font_texture_id, "You Destroyed", 0.5f, 0.05f,
            glm::vec3(-3.5f, 2.0f, 0.0f));
        draw_text(&g_shader_program, g_font_texture_id, "The Saibamen!", 0.5f, 0.05f,
            glm::vec3(-3.5f, 1.0f, 0.0f));
    }
    else if (state == moment::LOST) {
        draw_text(&g_shader_program, g_font_texture_id, "Yamcha Died", 0.5f, 0.05f,
            glm::vec3(-3.5f, 2.0f, 0.0f));
        draw_text(&g_shader_program, g_font_texture_id, "Again!!!", 0.5f, 0.05f,
            glm::vec3(-3.5f, 1.0f, 0.0f));
    }


    SDL_GL_SwapWindow(g_display_window);
}


void update()
{
    //temp bool to tell us to break block under

    g_state.player->check_collision_y(g_state.platforms,PLATFORM_COUNT);
    g_state.player->check_collision_x(g_state.platforms, PLATFORM_COUNT);
    

    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;

    delta_time += g_accumulator;

    if (delta_time < FIXED_TIMESTEP)
    {
        g_accumulator = delta_time;
        return;
    }

    while (delta_time >= FIXED_TIMESTEP)
    {
        g_state.player->update(FIXED_TIMESTEP, NULL, g_state.platforms, PLATFORM_COUNT);
        g_state.attack->update(FIXED_TIMESTEP, NULL, g_state.platforms, PLATFORM_COUNT);
        delta_time -= FIXED_TIMESTEP;
        for (int i = 0; i < 3; i++) {
            g_state.enemies[i].update(FIXED_TIMESTEP, NULL, g_state.platforms, PLATFORM_COUNT);
        }
    }

    g_accumulator = delta_time;
    if (ANGLE >= 360.0f) {
        ANGLE = 0.0f;
    } 


    for (int i = 0; i < 3; i++) {
        g_state.enemies[i].check_collision_y(g_state.platforms, PLATFORM_COUNT);
        g_state.enemies[i].check_collision_x(g_state.platforms, PLATFORM_COUNT);
        bool collided_with_player = g_state.enemies[i].check_collision(g_state.player);
        if (collided_with_player) {
            state = moment::LOST;
            break;
        }
        bool collided_with_sokidan = g_state.enemies[i].check_collision(g_state.attack);
        if (collided_with_sokidan) { g_state.enemies[i].deactivate(); g_enemy_count -= 1; g_score++; }
        else {
            if (g_state.enemies[i].get_collided_bottom() && g_state.enemies[i].get_entity_type() == ENEMY_JUMPER) {
                g_state.enemies[i].set_velocity(glm::vec3(0.0f, 20.0f, 0.0f));
            }
            if (g_state.enemies[i].get_entity_type() == ENEMY_ROTATOR) {
                g_state.enemies[i].rot_right();
                float angle = g_state.enemies[i].get_rot_angle();
            }
            if (g_state.enemies[i].get_entity_type() == ENEMY_FOLLOWER) {
                glm::vec3 player_pos = g_state.player->get_position();
                glm::vec3 my_pos = g_state.enemies[i].get_position();
                glm::vec3 direction = player_pos - my_pos;

                glm::normalize(direction);
                float speed = g_state.enemies[i].get_speed();
                my_pos += direction * speed * delta_time;

                g_state.enemies[i].set_position(my_pos);
            }
        }
        
    }

    // g_state.player->set_acceleration(glm::vec3(0.0f, -18.905f, 0.0f)); //attempting to fix goofy bug

    float plat_pos_x = plat_pos.x;
    bool win_condition = plat_pos_x - 0.5f < g_state.player->get_position().x < plat_pos_x + 0.5f;


    if (g_enemy_count <= 0) {
        state = moment::WON;
    }
    if (g_state.player->get_position().y < -3.5f || g_state.player->get_position().x > fabs(5.2f)) {
        state = moment::LOST;
    }
    //glm::vec3 player_pos = g_state.player->get_position();
    


    if (state == moment::WON) {//resets
        render();

        //delay(3000);
        state = moment::LAND;
        g_enemy_count = 3;
        //g_player_position = g_state.player->get_position();
        //delete g_state.player;
        delete[] g_state.platforms;
        //delete g_state.attack;
        delete[] g_state.enemies;
        initialise();
    }
    if (state == moment::LOST) {
        render();
        state = moment::LAND;
        delay(5000);
        //g_score = 0;
        //g_enemy_count = 3;
        delete g_state.player;
        delete[] g_state.platforms;
        delete g_state.attack;
        delete[] g_state.enemies;

        SDL_Quit();
    }

}



void shutdown()
{
    SDL_Quit();

    delete[] g_state.platforms;
    delete g_state.player;
    delete[] g_state.enemies;
    delete g_state.attack;
}

// ––––– GAME LOOP ––––– //
int main(int argc, char* argv[])
{
    std::cout << "program started\n";
    initialize_window();
    initialise();

    while (g_game_is_running)
    {
        if (pause == false) {
            process_input();
            update();
        }
        render();
    }

    shutdown();
    return 0;
}