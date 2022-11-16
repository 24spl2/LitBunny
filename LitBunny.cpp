#define _CRT_SECURE_NO_WARNINGS
#include "snail.cpp"
#include "cow.cpp"
#include "_cow_supplement.cpp"

void litBunny() {
    init();

    // shaders
    char *vert = R""""(
        #version 330 core

        layout (location = 0) in vec3 _p_model;
        layout (location = 1) in vec3 _n_model;

        uniform mat4 P, V, M;
        uniform float time;

        uniform bool draw_bad_normal_transform;

        out vec3 p_world;
        out vec3 _n_world;

        void main() {

            p_world = (M * vec4(_p_model, 1)).xyz;

            float walk = cos(time)*10 + p_world.z;
            float jump = cos(time) + p_world.y;
            p_world.z = walk;
            p_world.y = jump;

            _n_world = (transpose(inverse(M)) * vec4(_n_model, 0)).xyz; // correct normal transform
            if (draw_bad_normal_transform) { _n_world = (M * vec4(_n_model, 0)).xyz; } // _incorrect_ normal transform
            gl_Position = P * V * vec4(p_world, 1);
        }
    )"""";
    char *frag = R""""(
        #version 330 core

        out vec4 fragColor;

        uniform bool draw_color_by_normal;

        in vec3 p_world;
        in vec3 _n_world;

        uniform vec3 camera_origin;

        uniform int num_lights;
        uniform vec3 light_positions[16];
        uniform vec3 light_colors[16];

        float ambientStrength = 0.1;
        float specularStrength = 0.5;
        float diffuseStrength = 1;
        float shinyStrength = 16;

        void main() {


            vec3 n_world = normalize(_n_world); // question: why do we need to normalize?

            vec3 col;
            if (draw_color_by_normal) {
                col = .5 + .5 * n_world;
            } else {
                col = vec3(ambientStrength); // ambient light approximation
                for (int i = 0; i < num_lights; ++i) {

                    //ambient already built in by *.5 above
                    col *= light_colors[i];

                    //diffuse lighting
                    vec3 lightDir = normalize(light_positions[i] - p_world);
                    float diff = max(dot(n_world, lightDir), 0.0);
                    vec3 diffuse = diff * light_colors[i] * diffuseStrength;
                    col += diffuse;

                    //specular
                    vec3 viewDir = normalize(camera_origin - p_world);
                    //vec3 reflectDir = reflect(-lightDir, n_world);
                    vec3 halfwayDir = normalize(lightDir + viewDir);
                    float spec = pow(max(dot(n_world, halfwayDir), 0.0), shinyStrength);
                    vec3 specular = specularStrength * spec * light_colors[i];
                    col += specular;

                }
            }
            fragColor = vec4(col, 1);
        }
    )"""";
    int shader_program = shader_build_program(vert, frag);
    ASSERT(shader_program);

    // mesh
    FancyTriangleMesh3D fancy_bunny = load_fancy_mesh("data_fancy_bunny", true, true);
    int mesh_index = 0;
    FancyTriangleMesh3D *meshes[] = { &fancy_bunny, &meshlib.fancy_sphere };

    // lights
    #define MAX_NUM_LIGHTS 6
    int num_lights = 1;

    vec3 light_positions[MAX_NUM_LIGHTS] = {};
    vec3 light_colors[MAX_NUM_LIGHTS] = { monokai.red, monokai.orange, monokai.yellow, monokai.green, monokai.blue, monokai.purple };
    {
        for (int i = 0; i < MAX_NUM_LIGHTS; ++i) {
            light_positions[i] = 3 * normalized(V3(util_random_double(-1, 1), util_random_double(-1, 1), util_random_double(-1, 1)));
        }
        light_positions[0] = { 0, 0, 3 };
    }
    bool draw_light_positions = true;

    // misc opengl
    GLuint VAO, VBO[2], EBO; {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(2, VBO);
        glGenBuffers(1, &EBO);
    }

    // misc
    Camera3D camera = { 10, RAD(45) };
    double time = 0;
    bool playing = false;
    bool draw_color_by_normal = false;
    bool draw_bad_normal_transform = false;

    // fornow
    srand(0);

    while (begin_frame()) {

        camera_move(&camera);
        mat4 P = camera_get_P(&camera);
        mat4 V = camera_get_V(&camera);
        mat4 M = Translation(0, -1, 0) * Scaling(1, 1 + .5 * sin(3 * time), 1) * Translation(0, 1, 0);

        { // imgui and widgets
            imgui_slider("mesh_index", &mesh_index, 0, NELEMS(meshes) - 1, 'h', 'l', true);
            imgui_slider("num_lights", &num_lights, 0, MAX_NUM_LIGHTS, 'j', 'k');


            imgui_checkbox("draw_color_by_normal", &draw_color_by_normal, 'z');
            imgui_checkbox("draw_bad_normal_transform", &draw_bad_normal_transform, 'x');
            imgui_checkbox("draw_light_positions", &draw_light_positions, 'c');
            imgui_checkbox("playing", &playing, 'p');
            if (imgui_button("reset", 'r')) {
                time = 0;
            }
            jank_widget_translate3D(P * V, num_lights, light_positions);
        }

        { // draw
            FancyTriangleMesh3D *mesh = meshes[mesh_index];

            if (draw_light_positions) {
                basic_draw(POINTS, P * V, num_lights, light_positions, light_colors);
            }
            glBindVertexArray(VAO);

            glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
            glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * 3 * sizeof(double), mesh->vertex_positions, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_DOUBLE, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(0);

            glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
            glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * 3 * sizeof(double), mesh->vertex_normals, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(1, 3, GL_DOUBLE, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(1);

            glUseProgram(shader_program);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * mesh->num_triangles * sizeof(int), mesh->triangle_indices, GL_DYNAMIC_DRAW);

            shader_set_uniform_mat4(shader_program, "P", P);
            shader_set_uniform_mat4(shader_program, "V", V);
            shader_set_uniform_mat4(shader_program, "M", M);
            shader_set_uniform_double(shader_program, "time", time);

            shader_set_uniform_vec3(shader_program, "camera_origin", camera_get_origin(&camera));
            shader_set_uniform_int(shader_program, "num_lights", num_lights);


            shader_set_uniform_array_vec3(shader_program, "light_positions", num_lights, light_positions);
            shader_set_uniform_array_vec3(shader_program, "light_colors", num_lights, light_colors);
            shader_set_uniform_bool(shader_program, "draw_color_by_normal", draw_color_by_normal);
            shader_set_uniform_bool(shader_program, "draw_bad_normal_transform", draw_bad_normal_transform);

            glDrawElements(GL_TRIANGLES, 3 * mesh->num_triangles, GL_UNSIGNED_INT, NULL);
        }

        if (playing) {
            time += 1. / 60;
        }
    }
}

int main() {
    litBunny()
    return 0;
}
