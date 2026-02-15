<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile doxygen_version="1.15.0">
  <compound kind="file">
    <name>CMakeLists.txt</name>
    <path></path>
    <filename>CMakeLists_8txt.html</filename>
    <member kind="function">
      <type></type>
      <name>cmake_minimum_required</name>
      <anchorfile>CMakeLists_8txt.html</anchorfile>
      <anchor>a4f4d5ebff84903d46308f5e904f46468</anchor>
      <arglist>(VERSION 3.16) set(PROJECT_NAME &quot;esp32_chess_v24&quot;) set(PROJECT_VERSION &quot;2.4.0&quot;) include($ENV</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>animation_task.c</name>
    <path>components/animation_task/</path>
    <filename>animation__task_8c.html</filename>
    <includes id="animation__task_8h" name="animation_task.h" local="yes" import="no" module="no" objc="no">animation_task.h</includes>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <includes id="led__task__simple_8h" name="led_task_simple.h" local="yes" import="no" module="no" objc="no">led_task_simple.h</includes>
    <includes id="led__mapping_8h" name="led_mapping.h" local="yes" import="no" module="no" objc="no">led_mapping.h</includes>
    <class kind="struct">chess_position_t</class>
    <member kind="define">
      <type>#define</type>
      <name>MAX_ANIMATIONS</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a99f9cfd68ef4237b568bda6e3e63312f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX_FRAMES</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a5b4055201d2d8170e179b1ceaa438b9c</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANIMATION_TASK_INTERVAL</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>aaaafe1fcaf34b610aa12a2b28705e111</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>FRAME_DURATION_MS</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a1d1bdfd77e8bb3d8df370e30f70dd329</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>animation_task_wdt_reset_safe</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a08e644be7a7cc71b0a4fcdde5ea8d1f5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_initialize_system</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a09488753d3a3be4415fb5e1bc44d1fb0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>animation_create</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a8f46cd6a1f375c8d19d1c4315cabdbfd</anchor>
      <arglist>(animation_task_type_t type, uint32_t duration_ms, uint8_t priority, bool loop)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_start</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>abb35a3172d5d24a24902916b1ecfa40f</anchor>
      <arglist>(uint8_t animation_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_stop</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>adc882a0b86428a7333c86eddd1f26b8d</anchor>
      <arglist>(uint8_t animation_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_pause</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a41183595937bafd37b4ba07c47b8c62d</anchor>
      <arglist>(uint8_t animation_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_resume</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a04765d28e82fce17f90d526a7ce60fc2</anchor>
      <arglist>(uint8_t animation_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_generate_wave_frame</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a03563b1b04565e45d3892b520708bd64</anchor>
      <arglist>(uint32_t frame, uint32_t color, uint8_t speed)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_generate_pulse_frame</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a348dfefe5bb753b16839d5215505e4f4</anchor>
      <arglist>(uint32_t frame, uint32_t color, uint8_t speed)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_generate_fade_frame</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>ad2116436a161de0d3c2aa663ef02d12e</anchor>
      <arglist>(uint32_t frame, uint32_t from_color, uint32_t to_color, uint32_t total_frames)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_generate_chess_pattern</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>afb64e912fbca18925c8cac41b6006f2c</anchor>
      <arglist>(uint32_t frame, uint32_t color1, uint32_t color2)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_generate_rainbow_frame</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a63112c5794b76845d9afff7eb6fd3259</anchor>
      <arglist>(uint32_t frame)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animate_path_with_interruption</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>ab537cab674b8a582c2ee24dfa0fd10c5</anchor>
      <arglist>(chess_position_t *path, int path_length, piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>check_for_move_interruption</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a0123299d5b5afc994cbb803c1f1b9aa2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>new_move_detected</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>ad4a9e6f0e9175a5f2f10c3536fb43308</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>chess_position_t *</type>
      <name>calculate_knight_path</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>afce4e9bc86ef7e438e2066f5c20e17a6</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col, int *path_length)</arglist>
    </member>
    <member kind="function">
      <type>chess_position_t *</type>
      <name>calculate_diagonal_path</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>aef79ecbdeed6b80aac570f45da8c041f</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col, int *path_length)</arglist>
    </member>
    <member kind="function">
      <type>chess_position_t *</type>
      <name>calculate_straight_path</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>aca2bc3d94fa18bedc3b83e79151283e1</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col, int *path_length)</arglist>
    </member>
    <member kind="function">
      <type>chess_position_t *</type>
      <name>calculate_direct_path</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a5e3bb756394f0aa6b8846678a266bee6</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col, int *path_length)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animate_piece_move_natural</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a5361d673dfa5c9b0568be0e4cac80444</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col, piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_request_interrupt</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a2b5de7f9e2ba904cd16a02a3add0b6d6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_execute_frame</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>afecf0d9f3014ec819b9d8d4efecc1ee5</anchor>
      <arglist>(animation_task_t *anim)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_send_frame_to_leds</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>aec58eeb9ffe892383adeef75473dc160</anchor>
      <arglist>(const uint8_t frame[CHESS_LED_COUNT_TOTAL][3])</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_execute_move_highlight</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a06e87351882b4abb2b090c3e3fad2098</anchor>
      <arglist>(animation_task_t *anim)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_execute_check_highlight</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>aeeb63457d09fa55c64a67b38e895ab4a</anchor>
      <arglist>(animation_task_t *anim)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_execute_game_over</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>aa4e0d21daa2bff68ffc49abd0995d812</anchor>
      <arglist>(animation_task_t *anim)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_process_commands</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a0060e7605dd42ba4e2041e1c4402f60e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_stop_all</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a19ff3a915b7dad334a1d306492667916</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_pause_all</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a70e168a5ba925ad0d8da01a49d34e75c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_resume_all</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a749cd1a19567e9b741fb320e1d440736</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_print_status</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a9731cdb2cdcf0234b35af6ba0b7b1402</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_test_all</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>ab87baa702421428476b149e35aa74ab9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>animation_get_name</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>ad90823f255171d2792d13345b96d9477</anchor>
      <arglist>(animation_task_type_t animation_type)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_print_progress</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a79382186a6a22377fff18924934c118a</anchor>
      <arglist>(const animation_task_t *anim)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_print_piece_move</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a1fc234eed1ddbb156d9c1d61d21abd52</anchor>
      <arglist>(const char *from_square, const char *to_square, const char *piece_name, float progress)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_print_check_status</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>afd9f1291c4f0584d7d02b26744a24475</anchor>
      <arglist>(bool is_checkmate, float progress)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_print_summary</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a817be6e29efbd666e7334307f7c5fe5a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_task_start</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a18ab65be2c5b763305a8d6c1fa886392</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static animation_task_t</type>
      <name>animations</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a3506bb72f29fcaebecde5c53bd6139f2</anchor>
      <arglist>[20]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>next_animation_id</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a2c59480d31f1d62c0da421b1b3826642</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>task_running</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a35141cb45623cb3b7b4d96da35ed1831</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>active_animation_count</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a87e4e638f9c673b363ba09ebff9fe0c7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>current_animation_index</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a261a1c96f2134fa07a122bcbaf14167b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static volatile bool</type>
      <name>animation_interrupted</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>af2889f0cc8ff05da5e068e7574ed5b3b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static SemaphoreHandle_t</type>
      <name>animation_interrupt_mutex</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a5579f35967ee72c55398c089594c9f69</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const uint32_t</type>
      <name>rainbow_colors</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a58427d18cd3eb18ee8c7802c2a5a7814</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>wave_frame</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>abcfc9b225da909e47ab361defc782a42</anchor>
      <arglist>[CHESS_LED_COUNT_TOTAL][3]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>pulse_frame</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>a63913cf3472539c76d9f3b05a1a49361</anchor>
      <arglist>[CHESS_LED_COUNT_TOTAL][3]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>fade_frame</name>
      <anchorfile>animation__task_8c.html</anchorfile>
      <anchor>afc4dd0f46762995c1147511282c4cb2d</anchor>
      <arglist>[CHESS_LED_COUNT_TOTAL][3]</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>animation_task.h</name>
    <path>components/animation_task/include/</path>
    <filename>animation__task_8h.html</filename>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <class kind="struct">animation_task_t</class>
    <member kind="enumeration">
      <type></type>
      <name>animation_task_type_t</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a3da732fa10b2f45510643d0280be899f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TASK_TYPE_WAVE</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a3da732fa10b2f45510643d0280be899fa0c5fd4a4c75f36be7d1b0f975877620e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TASK_TYPE_PULSE</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a3da732fa10b2f45510643d0280be899fa37b30fa5964a71f2389c25752c95b5ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TASK_TYPE_FADE</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a3da732fa10b2f45510643d0280be899fa89c574e35190cd5d6c0d47e735718bf6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TASK_TYPE_CHESS_PATTERN</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a3da732fa10b2f45510643d0280be899fa36b068a6cd9ebad07b0b04c9d5528a8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TASK_TYPE_RAINBOW</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a3da732fa10b2f45510643d0280be899fa0e3c85323cb56d38a7e5637310a1e358</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TASK_TYPE_MOVE_HIGHLIGHT</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a3da732fa10b2f45510643d0280be899faf3ac26dd7d754276ebf33e08f6d2b0f0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TASK_TYPE_CHECK_HIGHLIGHT</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a3da732fa10b2f45510643d0280be899fa99ac41500f34c216717dcfba121dd8f4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TASK_TYPE_GAME_OVER</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a3da732fa10b2f45510643d0280be899fa41dc2bcd67a8bfa1fb98d5d3f2f1717f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TASK_TYPE_CUSTOM</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a3da732fa10b2f45510643d0280be899fa6eb688ab2a8237f9f4486e04f62077d3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>animation_task_state_t</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a2dadc2f6c02f4b392e290071e65a8fee</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TASK_STATE_IDLE</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a2dadc2f6c02f4b392e290071e65a8feeaddfd8d22e83f4e1d0577a20a7e4d2363</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TASK_STATE_RUNNING</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a2dadc2f6c02f4b392e290071e65a8feea888a2d4fdc00f8ebde59baee1853a94c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TASK_STATE_PAUSED</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a2dadc2f6c02f4b392e290071e65a8feeaa8c07db5ba52dc0914c933d3c772ec1c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TASK_STATE_FINISHED</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a2dadc2f6c02f4b392e290071e65a8feeae94eadc0c8a29a2f169f00ed65656a08</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_task_start</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a18ab65be2c5b763305a8d6c1fa886392</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_initialize_system</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a09488753d3a3be4415fb5e1bc44d1fb0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>animation_create</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a8f46cd6a1f375c8d19d1c4315cabdbfd</anchor>
      <arglist>(animation_task_type_t type, uint32_t duration_ms, uint8_t priority, bool loop)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_start</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>abb35a3172d5d24a24902916b1ecfa40f</anchor>
      <arglist>(uint8_t animation_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_stop</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>adc882a0b86428a7333c86eddd1f26b8d</anchor>
      <arglist>(uint8_t animation_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_pause</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a41183595937bafd37b4ba07c47b8c62d</anchor>
      <arglist>(uint8_t animation_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_resume</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a04765d28e82fce17f90d526a7ce60fc2</anchor>
      <arglist>(uint8_t animation_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_generate_wave_frame</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a03563b1b04565e45d3892b520708bd64</anchor>
      <arglist>(uint32_t frame, uint32_t color, uint8_t speed)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_generate_pulse_frame</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a348dfefe5bb753b16839d5215505e4f4</anchor>
      <arglist>(uint32_t frame, uint32_t color, uint8_t speed)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_generate_fade_frame</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>ad2116436a161de0d3c2aa663ef02d12e</anchor>
      <arglist>(uint32_t frame, uint32_t from_color, uint32_t to_color, uint32_t total_frames)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_generate_chess_pattern</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>afb64e912fbca18925c8cac41b6006f2c</anchor>
      <arglist>(uint32_t frame, uint32_t color1, uint32_t color2)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_generate_rainbow_frame</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a63112c5794b76845d9afff7eb6fd3259</anchor>
      <arglist>(uint32_t frame)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_execute_frame</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>afecf0d9f3014ec819b9d8d4efecc1ee5</anchor>
      <arglist>(animation_task_t *anim)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_send_frame_to_leds</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>aec58eeb9ffe892383adeef75473dc160</anchor>
      <arglist>(const uint8_t frame[CHESS_LED_COUNT_TOTAL][3])</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_execute_move_highlight</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a06e87351882b4abb2b090c3e3fad2098</anchor>
      <arglist>(animation_task_t *anim)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_execute_check_highlight</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>aeeb63457d09fa55c64a67b38e895ab4a</anchor>
      <arglist>(animation_task_t *anim)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_execute_game_over</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>aa4e0d21daa2bff68ffc49abd0995d812</anchor>
      <arglist>(animation_task_t *anim)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_process_commands</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a0060e7605dd42ba4e2041e1c4402f60e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_stop_all</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a19ff3a915b7dad334a1d306492667916</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_pause_all</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a70e168a5ba925ad0d8da01a49d34e75c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_resume_all</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a749cd1a19567e9b741fb320e1d440736</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_print_status</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a9731cdb2cdcf0234b35af6ba0b7b1402</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_test_all</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>ab87baa702421428476b149e35aa74ab9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_test_system</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a213dccff1d89e8b9afa89882fe6b5df9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>animation_get_name</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>ad90823f255171d2792d13345b96d9477</anchor>
      <arglist>(animation_task_type_t animation_type)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_print_progress</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a79382186a6a22377fff18924934c118a</anchor>
      <arglist>(const animation_task_t *anim)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_print_piece_move</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a1fc234eed1ddbb156d9c1d61d21abd52</anchor>
      <arglist>(const char *from_square, const char *to_square, const char *piece_name, float progress)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_print_check_status</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>afd9f1291c4f0584d7d02b26744a24475</anchor>
      <arglist>(bool is_checkmate, float progress)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_print_summary</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a817be6e29efbd666e7334307f7c5fe5a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animate_piece_move_natural</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a5361d673dfa5c9b0568be0e4cac80444</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col, piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_request_interrupt</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>a2b5de7f9e2ba904cd16a02a3add0b6d6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>new_move_detected</name>
      <anchorfile>animation__task_8h.html</anchorfile>
      <anchor>ad4a9e6f0e9175a5f2f10c3536fb43308</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>button_task.c</name>
    <path>components/button_task/</path>
    <filename>button__task_8c.html</filename>
    <includes id="button__task_8h" name="button_task.h" local="yes" import="no" module="no" objc="no">button_task.h</includes>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <includes id="led__task__simple_8h" name="led_task_simple.h" local="yes" import="no" module="no" objc="no">led_task_simple.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>BUTTON_DEBOUNCE_MS</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a6954510507da9fee0d980bb85da3e819</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BUTTON_LONG_PRESS_MS</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a45b3bff679685a8c6af1c956bca3fb2e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BUTTON_DOUBLE_PRESS_MS</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a4c4031ddea9a11da95a52560993716f0</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>button_task_wdt_reset_safe</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>af88dd4237305bcbdee51952490e7d163</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_scan_all</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a5fb2a771d8252d20fc0ad4d97667d4df</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_simulate_press</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a133a427b3e47eeb0e857d1efd4bbfe57</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_simulate_release</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a2f22af1e62d703a6cd12cfa7d7058865</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_process_events</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>aedc6316fcaee517a7a57905a217d44b8</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_handle_press</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>aae052fa1d3a64d8c90bb3ec9dc3e4210</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_handle_release</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a52544805262bb1776fcdb432960eb7be</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_check_double_press</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a506457119e2e5819ca95c3e876de7d5e</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_send_event</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a9c76fbdf52fcd61692a0ace95907507f</anchor>
      <arglist>(uint8_t button_id, button_event_type_t event_type, uint32_t duration)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_update_led_feedback</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a558352d2b9df8e61bfada7d422249e64</anchor>
      <arglist>(uint8_t button_id, bool pressed)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_set_led_color</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>ad004f184d705cb6e12a079c8105867bc</anchor>
      <arglist>(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_process_commands</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a0e6a1befa86d6dcea575c1effa25fb6d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_reset_all</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a97c84f568244fb112e4828e2985529ec</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_print_status</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a1573d1d2a02b4e2b619b9ef45e09fda5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_test_all</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>ae20ae1cc0ee0a2af8cdae453a22296a2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_task_start</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>af93b7fa55287e83f6caf447e756de6d2</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>button_states</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>ab644cfce56fbfcbaca3802d25f67273a</anchor>
      <arglist>[CHESS_BUTTON_COUNT]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>button_previous</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a28dbe9fbe2dfb9d51fd77b0c4d659b85</anchor>
      <arglist>[CHESS_BUTTON_COUNT]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>button_press_time</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a6f815ddbe7cae209a2b8c0747cacd6f5</anchor>
      <arglist>[CHESS_BUTTON_COUNT]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>button_release_time</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a375e6c4eb3ffa573ce925aa95992c772</anchor>
      <arglist>[CHESS_BUTTON_COUNT]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>button_press_count</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a7777450c083b08f48924746081144e32</anchor>
      <arglist>[CHESS_BUTTON_COUNT]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>button_long_press_sent</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>af96174b76a989be40faf4979d06346a0</anchor>
      <arglist>[CHESS_BUTTON_COUNT]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>task_running</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a35141cb45623cb3b7b4d96da35ed1831</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>simulation_mode</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a740a5ff73f18495856ddbc99ad099a6b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>button_names</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a5a8451e4ed607d6e653e39c1122ae600</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>last_simulation_time</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>ae80d84f5500f4925d29d6c8ace6b3d99</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>current_simulation_button</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a9a038fde1eedca43f5bf501138201779</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>button_simulate_release_time</name>
      <anchorfile>button__task_8c.html</anchorfile>
      <anchor>a22523cfa1d76dbd5aa73c91814da9750</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>button_task.h</name>
    <path>components/button_task/include/</path>
    <filename>button__task_8h.html</filename>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <member kind="enumeration">
      <type></type>
      <name>button_command_t</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>a8de3d61b9ce7ac3aa54b5c740773749a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BUTTON_CMD_RESET</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>a8de3d61b9ce7ac3aa54b5c740773749aab087f951763dfe8096343e91583f716c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BUTTON_CMD_STATUS</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>a8de3d61b9ce7ac3aa54b5c740773749aa067222378ede91b20155161e53d1f26b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BUTTON_CMD_TEST</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>a8de3d61b9ce7ac3aa54b5c740773749aac3e1e97088df34ca580325c9a48a6b84</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_task_start</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>af93b7fa55287e83f6caf447e756de6d2</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_scan_all</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>a5fb2a771d8252d20fc0ad4d97667d4df</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_simulate_press</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>a133a427b3e47eeb0e857d1efd4bbfe57</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_simulate_release</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>a2f22af1e62d703a6cd12cfa7d7058865</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_process_events</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>aedc6316fcaee517a7a57905a217d44b8</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_handle_press</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>aae052fa1d3a64d8c90bb3ec9dc3e4210</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_handle_release</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>a52544805262bb1776fcdb432960eb7be</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_check_double_press</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>a506457119e2e5819ca95c3e876de7d5e</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_send_event</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>a9c76fbdf52fcd61692a0ace95907507f</anchor>
      <arglist>(uint8_t button_id, button_event_type_t event_type, uint32_t duration)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_update_led_feedback</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>a558352d2b9df8e61bfada7d422249e64</anchor>
      <arglist>(uint8_t button_id, bool pressed)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_set_led_color</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>ad004f184d705cb6e12a079c8105867bc</anchor>
      <arglist>(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_process_commands</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>a0e6a1befa86d6dcea575c1effa25fb6d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_reset_all</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>a97c84f568244fb112e4828e2985529ec</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_print_status</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>a1573d1d2a02b4e2b619b9ef45e09fda5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_test_all</name>
      <anchorfile>button__task_8h.html</anchorfile>
      <anchor>ae20ae1cc0ee0a2af8cdae453a22296a2</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>config_manager.c</name>
    <path>components/config_manager/</path>
    <filename>config__manager_8c.html</filename>
    <includes id="config__manager_8h" name="config_manager.h" local="yes" import="no" module="no" objc="no">config_manager.h</includes>
    <member kind="function">
      <type>esp_err_t</type>
      <name>config_manager_init</name>
      <anchorfile>config__manager_8c.html</anchorfile>
      <anchor>ad744426452d0ddd581a96e82f2369853</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>config_load_from_nvs</name>
      <anchorfile>config__manager_8c.html</anchorfile>
      <anchor>a75038f5476c578ca0c192468d004160c</anchor>
      <arglist>(system_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>config_save_to_nvs</name>
      <anchorfile>config__manager_8c.html</anchorfile>
      <anchor>a24aceb1224555fd605ab85c9b0842357</anchor>
      <arglist>(const system_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>config_apply_settings</name>
      <anchorfile>config__manager_8c.html</anchorfile>
      <anchor>ae753d20be4991e260b40fd757087ebab</anchor>
      <arglist>(const system_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>config_reset_to_defaults</name>
      <anchorfile>config__manager_8c.html</anchorfile>
      <anchor>ae3afd4faf59e0c9b138456675661de9e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>config_get_defaults</name>
      <anchorfile>config__manager_8c.html</anchorfile>
      <anchor>ab4ecf373d6de350322f0debacd3a145d</anchor>
      <arglist>(system_config_t *config)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>config__manager_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const system_config_t</type>
      <name>default_config</name>
      <anchorfile>config__manager_8c.html</anchorfile>
      <anchor>a8afb2db2bb5f0a2920f95df018b0f3ed</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>config_manager.h</name>
    <path>components/config_manager/include/</path>
    <filename>config__manager_8h.html</filename>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>CONFIG_DEFAULT_VERBOSE_MODE</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>aa0028fc365e89e240fa5fbf5fd10dc80</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONFIG_DEFAULT_QUIET_MODE</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>a28d3ba84ebd4728150a80005d7011e52</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONFIG_DEFAULT_LOG_LEVEL</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>ae1a68f8dad797265d45abe06a259b45f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONFIG_DEFAULT_COMMAND_TIMEOUT</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>a738c632d7567c38a428c149e9b60c78c</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONFIG_DEFAULT_ECHO_ENABLED</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>a47c2b95d1d8288422494efa813aac56d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONFIG_DEFAULT_BRIGHTNESS</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>ab761ad48a054d663ba21852111fdc330</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONFIG_NVS_NAMESPACE</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>a3ec1858d742e64d0c66dfd9631302d11</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONFIG_NVS_KEY_VERBOSE</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>a1465441aad59c8e924e7d848436e88c8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONFIG_NVS_KEY_QUIET</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>ae2b5da40b2f1c4930562a5d61ce8b408</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONFIG_NVS_KEY_LOG_LEVEL</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>a3351759d56cb033763497863590ced96</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONFIG_NVS_KEY_TIMEOUT</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>a5e5a923fe1f235adbf87c70147123bdc</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONFIG_NVS_KEY_ECHO</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>a2ee245346700426e01e61948da5a9f8e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONFIG_NVS_KEY_BRIGHTNESS</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>a1bb567a5bd367abf7442aec292621481</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>config_manager_init</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>ad744426452d0ddd581a96e82f2369853</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>config_reset_to_defaults</name>
      <anchorfile>config__manager_8h.html</anchorfile>
      <anchor>ae3afd4faf59e0c9b138456675661de9e</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>enhanced_castling_system.c</name>
    <path>components/enhanced_castling_system/</path>
    <filename>enhanced__castling__system_8c.html</filename>
    <includes id="enhanced__castling__system_8h" name="enhanced_castling_system.h" local="yes" import="no" module="no" objc="no">enhanced_castling_system.h</includes>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <includes id="game__led__animations_8h" name="game_led_animations.h" local="yes" import="no" module="no" objc="no">game_led_animations.h</includes>
    <includes id="led__task_8h" name="led_task.h" local="yes" import="no" module="no" objc="no">led_task.h</includes>
    <includes id="led__mapping_8h" name="led_mapping.h" local="yes" import="no" module="no" objc="no">led_mapping.h</includes>
    <member kind="function">
      <type>esp_err_t</type>
      <name>enhanced_castling_init</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a61f43ef55c76d2c5863819a96679f86d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>enhanced_castling_start</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a68473225caaf82fb02dbe69dd2b0ac90</anchor>
      <arglist>(player_t player, bool is_kingside)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>enhanced_castling_handle_king_lift</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a71c6334fe7eed0d589acfeb1cb6aeaaf</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>enhanced_castling_handle_king_drop</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>ac1a783168547602db601cce5f25f5ef6</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>enhanced_castling_handle_rook_lift</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a1298ed566772765b083158dd04d7617e</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>enhanced_castling_handle_rook_drop</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>ad0b372fd30752be5c2817d04115ad28d</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>enhanced_castling_cancel</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a84967829c955ce40cb552af8f336c88d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>enhanced_castling_is_active</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a551d56ee547e30c59358ecfbef86302b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>castling_phase_t</type>
      <name>enhanced_castling_get_phase</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a573e41c1cc049dbb2722bc68b25f174b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>enhanced_castling_update_phase</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a2addf80390a5195a77495585144214c4</anchor>
      <arglist>(castling_phase_t new_phase)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>enhanced_castling_handle_error</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a0996145f3c92961e6d97861fb01e0fff</anchor>
      <arglist>(castling_error_t error, uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_calculate_positions</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a7fea86462c2fa344578489036f7fb9eb</anchor>
      <arglist>(player_t player, bool is_kingside)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>castling_validate_king_move</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>ae7bdbea7f5cfb9978bd0603bc5f4b4d0</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>castling_validate_rook_move</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a1669c97b2e5dcb839542b59392dbacbd</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>castling_validate_sequence</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a104226173b83a62df2d44053ed147131</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>castling_is_timeout_expired</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a3e4dc75c38e98d97cf35f984fa66b2ce</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_reset_system</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>af27a9e2af8a688e4ef4f6ea423ff5952</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_log_state_change</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>aee86001fa49a0428f9241c1e7427069b</anchor>
      <arglist>(const char *action)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_show_king_guidance</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>aef98d7b498beb12daa3096022ec887e7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_show_rook_guidance</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>ad5dc5a20cfdaf9d84030f5072778aff1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_show_error_indication</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>af9ae62a763763e34333532307fe30289</anchor>
      <arglist>(castling_error_t error)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_show_completion_celebration</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a6ccaa8b8a98da9de19017ad2fc611628</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_clear_all_indications</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>ae909113298c4be97e6a36572d3efeb4f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_recover_king_wrong_position</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>afe8c7b5de0f3719166f89e975059247c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_recover_rook_wrong_position</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a61e387e4475acd4a6c9db96fc1333e1f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_recover_timeout_error</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>ae58c1122476af928bccd44407c0e5822</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_show_correct_positions</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a12f1ed6212174d4f9532880ea94601a3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_show_tutorial</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a4733364aef5fb97b725594cada6e7679</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>enhanced_castling_system_t</type>
      <name>castling_system</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a3e1c3a621bec2612240013522edf0638</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>castling_led_config_t</type>
      <name>castling_led_config</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>ad5f9d23be7ccc7853164b1b290e6ad84</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const castling_error_info_t</type>
      <name>error_info</name>
      <anchorfile>enhanced__castling__system_8c.html</anchorfile>
      <anchor>a5a197b52b2c4d4ef9749cf548d08f361</anchor>
      <arglist>[]</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>enhanced_castling_system.h</name>
    <path>components/enhanced_castling_system/include/</path>
    <filename>enhanced__castling__system_8h.html</filename>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <includes id="game__led__animations_8h" name="game_led_animations.h" local="yes" import="no" module="no" objc="no">game_led_animations.h</includes>
    <class kind="struct">castling_positions_t</class>
    <class kind="struct">castling_led_state_t</class>
    <class kind="struct">castling_led_config_t</class>
    <class kind="struct">castling_error_info_t</class>
    <class kind="struct">enhanced_castling_system_t</class>
    <member kind="enumeration">
      <type></type>
      <name>castling_phase_t</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a5488d560e2ddb85e20098a6a750ff984</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_STATE_IDLE</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a5488d560e2ddb85e20098a6a750ff984adab804b233835495e7fc3d21592a228a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_STATE_KING_LIFTED</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a5488d560e2ddb85e20098a6a750ff984a6b8198b3c2a1bd0fc46f7d18dbfc04a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_STATE_KING_MOVED_WAITING_ROOK</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a5488d560e2ddb85e20098a6a750ff984ac9da4000210f2ea7dcbde1e49ecad0df</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_STATE_ROOK_LIFTED</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a5488d560e2ddb85e20098a6a750ff984a1dc5694d8891c142687e57791894626c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_STATE_COMPLETING</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a5488d560e2ddb85e20098a6a750ff984afb7f4437b4f3c44a900f182d8757bd52</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_STATE_ERROR_RECOVERY</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a5488d560e2ddb85e20098a6a750ff984a0ddeffb101a9b2c9a7a288bbb22b714e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_STATE_COMPLETED</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a5488d560e2ddb85e20098a6a750ff984a8de3374bdedd9647eee7fc63b937ba9d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>castling_error_t</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a9efb9de3b24e4bcd4d5307a6ce23cbd1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_ERROR_NONE</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a9efb9de3b24e4bcd4d5307a6ce23cbd1a573c50f91e017666758f1305c0b66dd4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_ERROR_WRONG_KING_POSITION</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a9efb9de3b24e4bcd4d5307a6ce23cbd1a1db8f8d658faee09a9c932202bb42cce</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_ERROR_WRONG_ROOK_POSITION</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a9efb9de3b24e4bcd4d5307a6ce23cbd1a722a013d2eb253c852c9903fbb05fa50</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_ERROR_TIMEOUT</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a9efb9de3b24e4bcd4d5307a6ce23cbd1aad564a60d4b825e5595c9d6375cae76e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_ERROR_INVALID_SEQUENCE</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a9efb9de3b24e4bcd4d5307a6ce23cbd1a90e0b9f36c173915167b09f99a664084</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_ERROR_HARDWARE_FAILURE</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a9efb9de3b24e4bcd4d5307a6ce23cbd1a38d391c73001939d7a3b2f25f8d0f293</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_ERROR_GAME_STATE_INVALID</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a9efb9de3b24e4bcd4d5307a6ce23cbd1ae1237a1a1e6880432a366a357f452a3c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CASTLING_ERROR_MAX_ERRORS_EXCEEDED</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a9efb9de3b24e4bcd4d5307a6ce23cbd1a7df1f2f16b2c41d3e5b7f92eb9862eaf</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>enhanced_castling_init</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a61f43ef55c76d2c5863819a96679f86d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>enhanced_castling_start</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a68473225caaf82fb02dbe69dd2b0ac90</anchor>
      <arglist>(player_t player, bool is_kingside)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>enhanced_castling_handle_king_lift</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a71c6334fe7eed0d589acfeb1cb6aeaaf</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>enhanced_castling_handle_king_drop</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>ac1a783168547602db601cce5f25f5ef6</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>enhanced_castling_handle_rook_lift</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a1298ed566772765b083158dd04d7617e</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>enhanced_castling_handle_rook_drop</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>ad0b372fd30752be5c2817d04115ad28d</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>enhanced_castling_cancel</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a84967829c955ce40cb552af8f336c88d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>enhanced_castling_is_active</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a551d56ee547e30c59358ecfbef86302b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>castling_phase_t</type>
      <name>enhanced_castling_get_phase</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a573e41c1cc049dbb2722bc68b25f174b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>enhanced_castling_update_phase</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a2addf80390a5195a77495585144214c4</anchor>
      <arglist>(castling_phase_t new_phase)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>enhanced_castling_handle_error</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a0996145f3c92961e6d97861fb01e0fff</anchor>
      <arglist>(castling_error_t error, uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_show_king_guidance</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>aef98d7b498beb12daa3096022ec887e7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_show_rook_guidance</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>ad5dc5a20cfdaf9d84030f5072778aff1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_show_error_indication</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>af9ae62a763763e34333532307fe30289</anchor>
      <arglist>(castling_error_t error)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_show_completion_celebration</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a6ccaa8b8a98da9de19017ad2fc611628</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_clear_all_indications</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>ae909113298c4be97e6a36572d3efeb4f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>castling_validate_king_move</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>ae7bdbea7f5cfb9978bd0603bc5f4b4d0</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>castling_validate_rook_move</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a1669c97b2e5dcb839542b59392dbacbd</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>castling_validate_sequence</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a104226173b83a62df2d44053ed147131</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_recover_king_wrong_position</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>afe8c7b5de0f3719166f89e975059247c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_recover_rook_wrong_position</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a61e387e4475acd4a6c9db96fc1333e1f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_recover_timeout_error</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>ae58c1122476af928bccd44407c0e5822</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_show_correct_positions</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a12f1ed6212174d4f9532880ea94601a3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_show_tutorial</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a4733364aef5fb97b725594cada6e7679</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_calculate_positions</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a7fea86462c2fa344578489036f7fb9eb</anchor>
      <arglist>(player_t player, bool is_kingside)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>castling_is_timeout_expired</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a3e4dc75c38e98d97cf35f984fa66b2ce</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_reset_system</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>af27a9e2af8a688e4ef4f6ea423ff5952</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>castling_log_state_change</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>aee86001fa49a0428f9241c1e7427069b</anchor>
      <arglist>(const char *action)</arglist>
    </member>
    <member kind="variable">
      <type>enhanced_castling_system_t</type>
      <name>castling_system</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>a3e1c3a621bec2612240013522edf0638</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>castling_led_config_t</type>
      <name>castling_led_config</name>
      <anchorfile>enhanced__castling__system_8h.html</anchorfile>
      <anchor>ad5f9d23be7ccc7853164b1b290e6ad84</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>freertos_chess.c</name>
    <path>components/freertos_chess/</path>
    <filename>freertos__chess_8c.html</filename>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <includes id="button__task_8h" name="button_task.h" local="yes" import="no" module="no" objc="no">button_task.h</includes>
    <includes id="led__task_8h" name="led_task.h" local="yes" import="no" module="no" objc="no">led_task.h</includes>
    <includes id="shared__buffer__pool_8h" name="shared_buffer_pool.h" local="yes" import="no" module="no" objc="no">shared_buffer_pool.h</includes>
    <includes id="streaming__output_8h" name="streaming_output.h" local="yes" import="no" module="no" objc="no">streaming_output.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>COORDINATED_MUX_TASK_STACK_SIZE</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a53f20ff71110439851d63626a31312a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>validate_gpio_pin</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a1ceecb2e00128b59e0224f35608eaf6c</anchor>
      <arglist>(gpio_num_t pin, const char *pin_name)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_gpio_init</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>af8e4ec2ba6554d39c4f7c9574718efdc</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_led_init</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ad817d527142431531e86a3f54d66fefd</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_matrix_init</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>aa7e19c2234c5d2c6b224dbf5d73cd7a9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_button_init</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ab7324679dbbef995936d7af7aff3f086</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_hardware_init</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a41db379171eabe6fccb60019e0e60549</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_create_queues</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a918837616a81eb433243f2c1e109f73a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_create_mutexes</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a9bb91e5162ab07c0d6e18df6a5a9f43e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>__attribute__</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a43c4623c2553d09a59385aabe971d75d</anchor>
      <arglist>((unused))</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>coordinated_multiplex_task</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a00c0a35485fb3de6ba2a604e7eb9b566</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_scan_timer_callback</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a60ba01ed8c95cf7326c889ea1b6a2bc2</anchor>
      <arglist>(TimerHandle_t xTimer)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_scan_timer_callback</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a904cb90015996d3ad0532e71bc502318</anchor>
      <arglist>(TimerHandle_t xTimer)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_update_timer_callback</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ae46cffe2b1cbbb95b41ce73ada4e7ecd</anchor>
      <arglist>(TimerHandle_t xTimer)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_create_timers</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ad4a9b6f60cb5280918a47885121ea229</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_start_timers</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a315226a40faf8a392b6328ed7c26340f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_freertos_init</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>aadded4ebb2b88babdb6319afda065872</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_nvs_init</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a8d34f1ba2358585af189badda7f0d051</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_system_init</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ae3398aa451de52fedd3ac9e5830fca76</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_check_memory_health</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a1fc97de08523b766a876d7688a3bdae3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_monitor_tasks</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a0321f7ecdafb014dbfb05309bf810493</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>chess_print_system_info</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>acc6c5ec2b59b331d0d27ef56a9f0c462</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_uart_send_string</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a7fb9ad5c97d8450a447d81ae4c6199fe</anchor>
      <arglist>(const char *str)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_uart_printf</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>af0a70e7b77c14224b0efc66be5c59396</anchor>
      <arglist>(const char *format,...)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_led_set_pixel</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a6f66538255c01c03931e08eac42e6e8f</anchor>
      <arglist>(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_led_set_all</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>aba042f04f5d5fcb5bfb8f1ea039be359</anchor>
      <arglist>(uint8_t red, uint8_t green, uint8_t blue)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_led_clear</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>af2ee44ebdd95075569a9d32e7460ff7e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_led_show_board</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ab72ea43bb7c2a1d9dee3d16ee7702a35</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_led_button_feedback</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ae834a611972910ae29f806ee1edc6ac5</anchor>
      <arglist>(uint8_t button_id, bool available)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_matrix_scan</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ad875b05d9ca835122984b99c4d092a6a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_matrix_reset</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a97e4f6a8afd280c2e3a7c7708c823f26</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_matrix_get_status</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a082e770fd9ed752fea35d0597d2cc4de</anchor>
      <arglist>(uint8_t *status_array)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_button_scan</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a9fcb61aa21d096994b53489091a335bd</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_button_get_status</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a6611221bad87e54f59f9dd794145a88f</anchor>
      <arglist>(uint8_t *button_status)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_game_init</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a8fc661c051f0a660c3ace708482afdef</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_game_reset</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>afed3614bd3811e6c72f9fd0f0c6f605f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_game_get_status</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>aee0685887418c8f4406109069ea004de</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_memory_systems_init</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ac2f54e3f77005d32b1df12eca333ed3e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>matrix_event_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a1b93ff1badbca8962927cb6fd837463e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>matrix_command_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>abc01112e6afa1b3e029218e52b7c2196</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>matrix_response_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a9b3fea6e381f159f9e088382e4040d27</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>button_event_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a05107bbe85fd87a92dad9efd71dbd653</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>button_command_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a5f1958b9037083cf953f250fe1f364a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>uart_command_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a7e436e9dcca9a044bb9264755b4caa9e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>uart_response_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a02a776b41d5e50969e5cf68dcbbc2c69</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>game_command_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a4ed770cbe46604586e494bae84b62304</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>game_status_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a6dfc9956f36db9a85a34268c6e14b059</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>animation_command_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a6fa8af7d1c0b048b9f2c363a9fb2b288</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>animation_status_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ad2c6aca6dc859def2315ed1c26f11aad</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>screen_saver_command_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ad6e2650c9733357ca904426081ccee72</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>screen_saver_status_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a494d670fc3ed5c9bf7fc74a908a92712</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>web_command_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a642abc1652d446215a18d55b0517edba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>web_server_command_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>aaed4d7c5bc3e8140853498b965ec94e2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>web_server_status_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>aa17b405ee779d94d3b6954dd83712591</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>test_command_queue</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a849df7d33a2eeeb8383dab84223a7813</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SemaphoreHandle_t</type>
      <name>led_mutex</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a864f0cc9b1f2e30bd3c0e225e2c48552</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SemaphoreHandle_t</type>
      <name>matrix_mutex</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a80a74d10dccdaa933010d0e5cf7202d8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SemaphoreHandle_t</type>
      <name>button_mutex</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a17744dc0b4a21889e7e9527835edc068</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SemaphoreHandle_t</type>
      <name>game_mutex</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>af5298ceb88597b4b25328ac7e1af7ff0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SemaphoreHandle_t</type>
      <name>system_mutex</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a54b3156204f85b12fbf1a9c012730afc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TimerHandle_t</type>
      <name>matrix_scan_timer</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ad589ad22574a5fdb69645141759216c4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TimerHandle_t</type>
      <name>button_scan_timer</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>af58f98e02d7d24dcc49a348ea1f22e77</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TimerHandle_t</type>
      <name>led_update_timer</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a9bd4853603c685d45ba49977256c7567</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TimerHandle_t</type>
      <name>system_health_timer</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a2d90fd6adb05ddb54ae494a47e311309</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static TimerHandle_t</type>
      <name>coordinated_multiplex_timer</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a0042f9cdb2f225fa940e25e47b2610ec</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static TaskHandle_t</type>
      <name>coordinated_multiplex_task_handle</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a5a10fe5e0bb7faf063f64958f1f7304b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>system_initialized</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ab2336e816f0a31f0e3e85fba396e4340</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>hardware_initialized</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ae37ba709842162d9a8328959d0cb0e05</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>freertos_initialized</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a32091f1b25b0412e3f75b7fb3dff4b90</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const gpio_num_t</type>
      <name>matrix_row_pins</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>ad9e3ceb44aa3ff2deb6b45b6079cc7e8</anchor>
      <arglist>[8]</arglist>
    </member>
    <member kind="variable">
      <type>const gpio_num_t</type>
      <name>matrix_col_pins</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>a2d07c58a77430cba050b46dd496ce081</anchor>
      <arglist>[8]</arglist>
    </member>
    <member kind="variable">
      <type>const gpio_num_t</type>
      <name>promotion_button_pins_a</name>
      <anchorfile>freertos__chess_8c.html</anchorfile>
      <anchor>aa13f5879e99176ffada8295c49c67b6c</anchor>
      <arglist>[4]</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>chess_types.h</name>
    <path>components/freertos_chess/include/</path>
    <filename>chess__types_8h.html</filename>
    <class kind="struct">chess_move_t</class>
    <class kind="struct">chess_move_extended_t</class>
    <class kind="struct">chess_move_command_t</class>
    <class kind="struct">game_response_t</class>
    <class kind="struct">led_command_t</class>
    <class kind="struct">button_event_t</class>
    <class kind="struct">matrix_event_t</class>
    <class kind="struct">matrix_command_t</class>
    <class kind="struct">move_suggestion_t</class>
    <class kind="struct">system_config_t</class>
    <class kind="struct">led_animation_state_t</class>
    <member kind="define">
      <type>#define</type>
      <name>CHESS_LED_COUNT_BOARD</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a490b67efe5e74def79831ae40041f654</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHESS_LED_COUNT_BUTTONS</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab7815f5333fcf26dd13f9966d6564412</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHESS_LED_COUNT_TOTAL</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>afb4b5c24ea71da31d1fe1536f5c18728</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHESS_LED_COUNT</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a6e67f103d41bb597001bdc4f58650cb3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHESS_BUTTON_COUNT</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a6f0f1e32a1bc7c5e590c118b42f59b2c</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHESS_MATRIX_SIZE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a66570ebf9fc22729e105df1e70b02db5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX_MOVE_HISTORY</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a3b87426d2e6f6d464ec0a2df2f66dd83</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>piece_t</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab9be55f539b97c45fb9f57efded733c2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PIECE_EMPTY</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab9be55f539b97c45fb9f57efded733c2a78c1cf9cdf8f00b3f94fcc2f867180f1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PIECE_WHITE_PAWN</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab9be55f539b97c45fb9f57efded733c2af13b4b9f9748aed2bca41c5700956f2d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PIECE_WHITE_KNIGHT</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab9be55f539b97c45fb9f57efded733c2a4c99e4617db8d3f21a4dd23e497d0b0c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PIECE_WHITE_BISHOP</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab9be55f539b97c45fb9f57efded733c2ac5624ba607e63e7a6f7ebdab4b322992</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PIECE_WHITE_ROOK</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab9be55f539b97c45fb9f57efded733c2ac403d9dfd1cf2dc0be0217a0a54c0a42</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PIECE_WHITE_QUEEN</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab9be55f539b97c45fb9f57efded733c2a6d9b099ad26f6f85108b90494697d5dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PIECE_WHITE_KING</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab9be55f539b97c45fb9f57efded733c2a01fd9d2b562a7147017fd02542bd1411</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PIECE_BLACK_PAWN</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab9be55f539b97c45fb9f57efded733c2a153df31a0a287a4198f917f52a56be1b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PIECE_BLACK_KNIGHT</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab9be55f539b97c45fb9f57efded733c2a3888c0ae958fcf54d7b4f77a26bdaa60</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PIECE_BLACK_BISHOP</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab9be55f539b97c45fb9f57efded733c2ae844e749448cbcaae2fda6a56bb70b86</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PIECE_BLACK_ROOK</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab9be55f539b97c45fb9f57efded733c2a26b69f7adf92c62bfa744b9e77907584</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PIECE_BLACK_QUEEN</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab9be55f539b97c45fb9f57efded733c2a29e99921b92026f4b6bbeacf08289238</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PIECE_BLACK_KING</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ab9be55f539b97c45fb9f57efded733c2a026ec390a91a6f3858e8819b550c1fe6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>game_state_t</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4edce1ca040716922b6e4a79be4e414d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_STATE_IDLE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4edce1ca040716922b6e4a79be4e414dac440660deb460f2f6ad318818cd0458f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_STATE_INIT</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4edce1ca040716922b6e4a79be4e414da9ad0f6945756ecd872133c8d917cb30c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_STATE_ACTIVE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4edce1ca040716922b6e4a79be4e414da073e32e8ef26903af9aab57f672e893c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_STATE_PAUSED</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4edce1ca040716922b6e4a79be4e414dae6a757f0eb2e39168756440ac804ffa7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_STATE_FINISHED</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4edce1ca040716922b6e4a79be4e414da605d180d86561f11b5015a52dbf6d9f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_STATE_ERROR</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4edce1ca040716922b6e4a79be4e414daab30ad97fe24d756e3ed2c9f2420efc2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_STATE_PLAYING</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4edce1ca040716922b6e4a79be4e414dab8ee64fcce8c3b17312b20c78fc9695a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_STATE_PROMOTION</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4edce1ca040716922b6e4a79be4e414da755790e6ec07d2f80a27fab71e9c5735</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_STATE_ERROR_RECOVERY</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4edce1ca040716922b6e4a79be4e414dabf733d9340403aaf6b4fcf2fec57596a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_STATE_WAITING_FOR_RETURN</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4edce1ca040716922b6e4a79be4e414da8d38d078f83ba6d016773d797e914e80</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>player_t</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a49ff5e2d7800ecd3792c990f4445c3f1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PLAYER_WHITE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a49ff5e2d7800ecd3792c990f4445c3f1a3fc75a745f989dff2b00f6b96f5baa9f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PLAYER_BLACK</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a49ff5e2d7800ecd3792c990f4445c3f1af8db5eba569f0a6497497bf46037c98e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>game_result_type_t</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4d48e82714c3766a44b6ae5378386b10</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>RESULT_WHITE_WINS</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4d48e82714c3766a44b6ae5378386b10abb26be1a9e2d11602e9c5968e1a63602</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>RESULT_BLACK_WINS</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4d48e82714c3766a44b6ae5378386b10a0f3de4cbc4ca712f898984075e875990</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>RESULT_DRAW_STALEMATE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4d48e82714c3766a44b6ae5378386b10a05e8dcf1a732c2020d364fb5d9cea20e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>RESULT_DRAW_50_MOVE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4d48e82714c3766a44b6ae5378386b10a6577cffad8cbf7e5ca95dd46e504705b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>RESULT_DRAW_REPETITION</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4d48e82714c3766a44b6ae5378386b10a0486ad918f30f8bb5d8db5d92f72c208</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>RESULT_DRAW_INSUFFICIENT</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4d48e82714c3766a44b6ae5378386b10a330166862ce8a46d98a340b42435faad</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>move_error_t</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_NONE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1a4100d3557901ed1bf26cadf95436db60</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_INVALID_SYNTAX</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1a7a0d7fa395f1f1826ecf2859d4b1dfbc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_INVALID_PARAMETER</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1a811b7944c3ffe2044c660548f291c20b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_PIECE_NOT_FOUND</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1ab4cb258bbbe0300855b48b6f2d1b066f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_INVALID_MOVE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1afdd1c5dd74c3751769aee3f28589cdea</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_BLOCKED_PATH</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1a641b6ce57e59a5c718ca5ee3e51f9444</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_CHECK_VIOLATION</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1a2fb8b41d9e36545a7a9410d4b8528215</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_SYSTEM_ERROR</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1ab3b3ce37d8c4beef920d33cd4b10fbd1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_NO_PIECE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1ae25a34f37cd35bcd83411aecc8f439f1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_WRONG_COLOR</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1a5723bd10d7972babaac58c5fa122371e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_INVALID_PATTERN</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1a6dc786a7e8aea2d1f178c80c73848675</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_KING_IN_CHECK</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1a1efb8e4fd12884711d5de2950e74f087</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_CASTLING_BLOCKED</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1a062f25db8c860c564d19c9e46c776fe0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_EN_PASSANT_INVALID</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1ac71919eb046ab42ba1e6a622b914863a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_DESTINATION_OCCUPIED</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1a4dab65373947ddaa9d99838441f3fcff</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_OUT_OF_BOUNDS</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1a0d51fa502a0284e356d4c196152a9165</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_GAME_NOT_ACTIVE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1a98e7980c293506d89bac9ef4fae9e237</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_INVALID_MOVE_STRUCTURE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1ae99b00ccc116996ee3eb7886d97f72e1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_INVALID_COORDINATES</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1ad0f6ecbd8e5bf7852d4298269a812f33</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_ERROR_ILLEGAL_MOVE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad29cfdba222dc553f977a701312d18c1adbc56938feb4f9a70401c3190d77f9de</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>promotion_choice_t</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a1b67a105371c4a0066f77f3fa12ad237</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PROMOTION_QUEEN</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a1b67a105371c4a0066f77f3fa12ad237ace7ae3bfdb598aeffb32c01d08805045</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PROMOTION_ROOK</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a1b67a105371c4a0066f77f3fa12ad237a86adcf7f4dd13fe13c310740b684fd85</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PROMOTION_BISHOP</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a1b67a105371c4a0066f77f3fa12ad237af76338fd133ad75d03265525fab6a060</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PROMOTION_KNIGHT</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a1b67a105371c4a0066f77f3fa12ad237a38e92903ca7d679f019bbc0895c00910</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>move_type_t</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a049445f8ca6e7d0daf13a256e4375576</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_TYPE_NORMAL</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a049445f8ca6e7d0daf13a256e4375576a81d6017982215bfb2df43b04360c21f7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_TYPE_CAPTURE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a049445f8ca6e7d0daf13a256e4375576a0c1ae66eb0f22d3d3da0216c3c4a4c43</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_TYPE_CASTLE_KING</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a049445f8ca6e7d0daf13a256e4375576a348ebd44da63a3260560e30452bd3d4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_TYPE_CASTLE_QUEEN</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a049445f8ca6e7d0daf13a256e4375576a9402d70bce003961f9bef482a0f05abc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_TYPE_EN_PASSANT</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a049445f8ca6e7d0daf13a256e4375576a23c92ea1d63a461f09485c3762e3656d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MOVE_TYPE_PROMOTION</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a049445f8ca6e7d0daf13a256e4375576a0616f79303505961b7054938d27c5a0f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>game_command_type_t</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_NEW_GAME</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a9cdd3824f45d61a774ab6868e898e1c0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_RESET_GAME</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4afa32272119e942c6d9ecf8e41d53e0e0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_MAKE_MOVE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a4f5010e7796f733431849acc8175fdbe</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_UNDO_MOVE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a982ea3a5c5efd162ad30ebb49ca51216</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_GET_STATUS</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4ad280705c3d4a5140b36a1ed9bf9d0d45</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_GET_BOARD</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4ac834cfe65957284eaf2fc08fb85f82c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_GET_VALID_MOVES</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a605e6b57d3beaf8791ef5fdb55b4eb8c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_PICKUP_PIECE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4ac645ae7fb45f72e070133d0486796614</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_DROP_PIECE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4ab7951fcd18e6890d22362a1ab5e2c264</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_PROMOTION</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a555e98c01f0511c7d341e43169c820da</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_MOVE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4aac9a76d6ee39f51d9053539f833fa9e7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_SHOW_BOARD</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a4beaa7c5ed6b0fe21e26bd42e5b7212f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_PICKUP</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a745816733d012d70b4bda9d2fdac7892</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_DROP</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4aecfd44979b16dd3a0a3054c1eeda3102</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_GET_HISTORY</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a154ca18e7adccc46914b5716a7d720d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_DEBUG_INFO</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a5bd7b3ab11b01c056e66c73f3f16ccd7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_DEBUG_BOARD</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a52a8afc72ca379bd08e7b0796c35f92c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_EVALUATE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4acb7c84980c90b6f63746d28c96046913</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_SAVE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a3161112d062f6a357e99eb9bf11cb44b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_LOAD</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4aee4f4884065227d8f092688459822142</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_RESERVED_SLOT_20</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4ab8bc2ab1bed58cfad662329a579bbc03</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_CASTLE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4ad4ee83d72efd15438c23da18cef27702</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_PROMOTE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4afc983203f792a050205d8e7c320a57a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_COMPONENT_OFF</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a1049ce6ea8c65578b4a23f3ddb587725</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_COMPONENT_ON</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4ae7a0a5eaed97f55f06722787f7c8e653</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_ENDGAME_WHITE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a6fcaa25e964049b2db00af284ec8e02d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_ENDGAME_BLACK</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4afc04543eb95bbf50a58afcd3fd75e977</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_LIST_GAMES</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a476e5fbe5ef283560d72a139725a2f65</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_DELETE_GAME</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4ac25c5694beb04bb57b2167c15813cf6d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_RESERVED_SLOT_29</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a0f27b5a8f5a2258a2ed7f623666d79f8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_RESERVED_SLOT_30</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4ad03f8d3baafcac16fa47ad548188c867</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_RESERVED_SLOT_31</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a411ad479086cc92829a2781eafc0244e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_RESERVED_SLOT_32</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4ab39fccde275812aa7e9f3df3d459d3bc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_TEST_MOVE_ANIM</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a4df6d159b32b16a21a371bf19c195d56</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_TEST_PLAYER_ANIM</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a6cd41ae730de32de23029864ab1ece8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_TEST_CASTLE_ANIM</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a4f003c07c6a984387da799f3be48acac</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_TEST_PROMOTE_ANIM</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a0787063d7206bb845726e1b029bd0fce</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_TEST_ENDGAME_ANIM</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4af524d161e2c33943da0a59805c227e2a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_RESERVED_SLOT_38</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4aa2309d8e560fce089e0d74ef3dbc65b8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_SET_TIME_CONTROL</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a0ba09395c8b55913ce75adb28ac19003</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_PAUSE_TIMER</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a04714aa90de7f3ace2585fe01cf67659</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_RESUME_TIMER</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a19a04a30f9a17d5a85cd43db2b1e298a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_RESET_TIMER</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4aee7fca1d1758d39255d3b023f569313d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_GET_TIMER_STATE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4ae152c62fc7eae57cbf8fc14a3ff749e2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_CMD_TIMER_TIMEOUT</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ade7744437207164be6ca650b128bd9e4a1963a76107560b25595a00d47b103534</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>game_response_type_t</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a03211d5ca1b7406c1cb6ab6851e79502</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_RESPONSE_SUCCESS</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a03211d5ca1b7406c1cb6ab6851e79502a422f73724b8ecc76f312621fb868805d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_RESPONSE_ERROR</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a03211d5ca1b7406c1cb6ab6851e79502a39ae193ecb15c805e19efa3c004cda8b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_RESPONSE_BOARD</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a03211d5ca1b7406c1cb6ab6851e79502a8379a8a9c1014d84b3fa379e9847f380</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_RESPONSE_MOVES</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a03211d5ca1b7406c1cb6ab6851e79502a59aa3569c7b540e83b61a3a1b356598f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_RESPONSE_STATUS</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a03211d5ca1b7406c1cb6ab6851e79502a3bf05d8245c183a13405dfabe11fe238</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_RESPONSE_HISTORY</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a03211d5ca1b7406c1cb6ab6851e79502a081aadcf7717c7bdc4e86232c8c8185a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_RESPONSE_MOVE_RESULT</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a03211d5ca1b7406c1cb6ab6851e79502ae719c242921ba6e51afbe5488a523486</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GAME_RESPONSE_LED_STATUS</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a03211d5ca1b7406c1cb6ab6851e79502abb993899f97b2c62c6d1ede698a04a81</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>led_command_type_t</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_SET_PIXEL</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da8f40e7b12399c8f0cec1d5f78c950577</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_SET_ALL</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66dab6091468d24a1e3d1335390f4c74bfcd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_CLEAR</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da476971a6dcd45da3995eb36854baecb7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_SHOW_BOARD</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66dad724b31a2397df9b3d6814ff1974e724</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_BUTTON_FEEDBACK</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66daf3a2d5ed1608b52c41c9ccd1845c4899</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_BUTTON_PRESS</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66dac62e21d53111889c8fe6c35854548985</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_BUTTON_RELEASE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da71b61e2de4e8d597a55d95f3ab6f81ed</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_ANIMATION</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66dab94ae0805f7899148694194f7b562b74</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_TEST</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66dabefc2d40076aacd32ffd40c07fc8afe5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_TEST_ALL</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da88da737e1611d2de1a3b8d44fb559b38</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_MATRIX_OFF</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66dabb0452beba547aeaff3617f027ee0221</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_MATRIX_ON</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da12e33eb2fd816cc6d9775a3c69b3ce70</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_RESERVED_SLOT_13</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da02493a700b3bcd46cde391cb7b84966f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_RESERVED_SLOT_14</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da99cc49c9086cca2b4d67e431f6f8243e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_RESERVED_SLOT_15</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da01e63f99b685e64e8d0e8c356b59f9e2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_RESERVED_SLOT_16</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da6f598eb95d085991a9c7210c7d9f4fbe</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_RESERVED_SLOT_17</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da5ba99e2216abb36215c2d19fa7273c7a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_RESERVED_SLOT_18</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66daea821d2bde7f03358eecf371942fb6be</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_ANIM_PLAYER_CHANGE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66dac0069a9eecc579e3bcc3c14d6865fe28</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_ANIM_MOVE_PATH</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66dab4580f84f360910c3c26cd55b1ecdf42</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_ANIM_CASTLE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66dac134d8980d40f9fc8a12f683b967b187</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_ANIM_PROMOTE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da08d8d77aeb5693180fb3c7752dccd626</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_ANIM_ENDGAME</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66daf8bf66ffd150099b9aed3c2e8a54d430</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_ANIM_CHECK</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da09320b788323ec0c6c037fbe5405fc94</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_ANIM_CHECKMATE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66daa88ade2b1617bd7e2dbb98a0fd8f93ea</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_RESERVED_SLOT_26</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da0a3b3480bea2aedbb8ba9ba174a97afc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_DISABLE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da3245bcef006f573a1e0c42e15e75ac3e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_ENABLE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da0d4704d7f14b5966b4d5e1e15c8effbe</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_BUTTON_PROMOTION_AVAILABLE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66daf8124758b9a58effb1393e3482358b1f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_BUTTON_PROMOTION_UNAVAILABLE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da441731e024d2f2a63ff838654017cb6f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_BUTTON_SET_PRESSED</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da1cd4129febc39d32125f7b63cbb1b968</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_BUTTON_SET_RELEASED</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da645e84d2ccec89c75209784f912340d8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_GAME_STATE_UPDATE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66dae4852702ef287c906e2ca5272c047361</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_HIGHLIGHT_PIECES</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66daf0f30b49574874673f7dedf4e3ef637b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_HIGHLIGHT_MOVES</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66dad5d7870e73aceb328b67166210b27182</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_CLEAR_HIGHLIGHTS</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da9188b02c7998356975109115ec629b21</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_PLAYER_CHANGE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66daa5c435ea8239bab77cfab55550b5bee1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_ERROR_INVALID_MOVE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da61126ce66596b43a74b040d15ae4647c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_ERROR_RETURN_PIECE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66dab074e02d4c9516005c3bd7d31f8e787b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_ERROR_RECOVERY</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da6b00fc5d44a9d18c1cb99bb13c0f5815</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_SHOW_LEGAL_MOVES</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da55cf1cd112d68a2c834a4a8b83f26415</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_CASTLING_GUIDANCE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da58cefa5ccaee7e155c8f0ee780209e39</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_CASTLING_ERROR</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da279bafa5fec225b86ef70ef4ecdd2970</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_CASTLING_CELEBRATION</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da3e683c2a42a2ca7ed7a715d2492fee85</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_CASTLING_TUTORIAL</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da0acbe04632886e7fcc679174560cac06</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_CASTLING_CLEAR</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66daff455cc7d17fa43994e64e9c9b5e663f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_HIGHLIGHT_HINT</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66daf9dfd49be96d73059bdcf3f4e30c42dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_STATUS_ACTIVE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66daa1b0d419ef172adea7e5d1c4f93b4c69</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_STATUS_COMPACT</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da79fc9577fe569938b3650e0615820b32</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_STATUS_DETAILED</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66da4c9252d50be03c29a85aff96f4db49a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_CMD_SET_BRIGHTNESS</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4b9c35d83af38787d5601d9c2443a66dae081bd5b21647bf52eb2bfb33e97a390</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>button_event_type_t</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a54c67f8a6d26165b932e66b06b5b904b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BUTTON_EVENT_PRESS</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a54c67f8a6d26165b932e66b06b5b904bac5a8f24e3beed7990f5495be58d2dc79</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BUTTON_EVENT_RELEASE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a54c67f8a6d26165b932e66b06b5b904baf7f5f99d0588a4d7a637bdc77a0c1172</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BUTTON_EVENT_LONG_PRESS</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a54c67f8a6d26165b932e66b06b5b904ba457eb030194c21ed5a0b670f53a00758</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BUTTON_EVENT_DOUBLE_PRESS</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a54c67f8a6d26165b932e66b06b5b904ba2629ec0f6865d8edfac4994530fb4555</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>matrix_event_type_t</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a86af72d7b967d33d0fade767c033320f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MATRIX_EVENT_PIECE_LIFTED</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a86af72d7b967d33d0fade767c033320fa39129c9582541c86108746c63a6be30d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MATRIX_EVENT_PIECE_PLACED</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a86af72d7b967d33d0fade767c033320fae68c93c0c4fc7f3f100942775984e594</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MATRIX_EVENT_MOVE_DETECTED</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a86af72d7b967d33d0fade767c033320fa35b920398a95b8a2cb0b48a31df01619</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MATRIX_EVENT_ERROR</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a86af72d7b967d33d0fade767c033320fabb7b9f5963909feca6e37f45eda97246</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>matrix_command_type_t</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a2e80d860c07cd8da638b44dbf01e447a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MATRIX_CMD_SCAN</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a2e80d860c07cd8da638b44dbf01e447aa41ace29c9ff45d2dfb21f28e298c7943</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MATRIX_CMD_RESET</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a2e80d860c07cd8da638b44dbf01e447aa7bc413e23a6bc09a2e155096fa603dec</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MATRIX_CMD_TEST</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a2e80d860c07cd8da638b44dbf01e447aa2fff98c7f65532e3805fa0aa51874280</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MATRIX_CMD_CALIBRATE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a2e80d860c07cd8da638b44dbf01e447aa5942ac097cba4821815b332686b8224d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MATRIX_CMD_DISABLE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a2e80d860c07cd8da638b44dbf01e447aa7623c181ba460f2efa8fc0704a809700</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MATRIX_CMD_ENABLE</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a2e80d860c07cd8da638b44dbf01e447aac28eb91d87621eebcfe95be8e6a736ad</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_valid_square</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad58024344b5777da494b4a35f8250c32</anchor>
      <arglist>(int row, int col)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_own_piece</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a7f31b3ea42f40399386306ba3d88cb9c</anchor>
      <arglist>(piece_t piece, player_t player)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_enemy_piece</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a4a1fae8d66a8db2dd28a091a3b11279c</anchor>
      <arglist>(piece_t piece, player_t player)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_simulate_move_check</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a3d771c47f735ab5e75fee68915b073fc</anchor>
      <arglist>(chess_move_extended_t *move, player_t player)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>config_manager_init</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ad744426452d0ddd581a96e82f2369853</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>config_load_from_nvs</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a75038f5476c578ca0c192468d004160c</anchor>
      <arglist>(system_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>config_save_to_nvs</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>a24aceb1224555fd605ab85c9b0842357</anchor>
      <arglist>(const system_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>config_apply_settings</name>
      <anchorfile>chess__types_8h.html</anchorfile>
      <anchor>ae753d20be4991e260b40fd757087ebab</anchor>
      <arglist>(const system_config_t *config)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>freertos_chess.h</name>
    <path>components/freertos_chess/include/</path>
    <filename>freertos__chess_8h.html</filename>
    <includes id="chess__types_8h" name="chess_types.h" local="yes" import="no" module="no" objc="no">chess_types.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>CHESS_SYSTEM_NAME</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>aa46150276ebae8e1eec4efdc53dffce4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHESS_SYSTEM_VERSION</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a204a7ee69d817206b7510c6ffc47643a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHESS_SYSTEM_AUTHOR</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>afbda35de111776579b8d9abad085db4d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHESS_VERSION_STRING</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>acc0d1902b8ef5d4934f99724427e5bc4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHESS_BUILD_DATE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a8b1a270b4e32e22a214ff3ad8e670ef7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_DATA_PIN</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a5f55b07707df2f2cf371f707207ed508</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STATUS_LED_PIN</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a089a71f836911c71b3f73fdd3b4b890b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_ROW_0</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a412cf595c5d992d528f572367cd382fc</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_ROW_1</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a26f828d1911fdbed9f5a190d38b869f4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_ROW_2</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a6c3c544fb9cfc45bc5c9b321be7cd410</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_ROW_3</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>aa33e7a34678ad6fe782210f99a7672ad</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_ROW_4</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a71559152b07023bcb341ac1a2b1a6f42</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_ROW_5</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a42d9ffa9b417036ce69c56a7b895bebb</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_ROW_6</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a5f70e9694a3ea89ce6c9c4396843a3b2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_ROW_7</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ae9ab626eb119d23769111f56a1bcaa16</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_COL_0</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a3e029f70c5d00d3e249faf7804581b0b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_COL_1</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ac18820617d1dcd813d8ac903a5c93a8d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_COL_2</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a141d2717e84ae5d86135f4562f67481b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_COL_3</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>af876ae3f9c7de7f9528648519939c0ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_COL_4</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ab799338bcbcd21959ba40e084d6b5327</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_COL_5</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a712d6392405fecb625bb7543219ac7a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_COL_6</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a5c56175a56a0013ce45265301301e937</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_COL_7</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a2194cd55ef55fe74673eff35a72c2b4b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BUTTON_RESET</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a0710f0869f3bcc7820964e85f3d59fa4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BUTTON_QUEEN</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a123381859161caa7858faf29b48e26e5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BUTTON_ROOK</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a7af87dab7ead0a04fb6403b906db7ae0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BUTTON_BISHOP</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ac861d083f88a75aef0cb5a65ba142f97</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BUTTON_KNIGHT</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>aa4b9945e8a7178906001d2393d6c069d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_SCAN_TIME_MS</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ab3d042ad7a50f8a44015cc2a749d3fa4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BUTTON_SCAN_TIME_MS</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a3f22d026310bf8c1f12bbed516a074ea</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>TOTAL_CYCLE_TIME_MS</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a519b0c7ea34c85b7e4c7144095820e65</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SYSTEM_HEALTH_TIME_MS</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>aeb05d15370ffa97f0d3ca135ffbc3bc9</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_COMMAND_TIMEOUT_MS</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>afe966dccff7e1c439146a85369c623f7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_MUTEX_TIMEOUT_MS</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>aa31a93cf587ac835b9458a02582a2ff4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_HARDWARE_UPDATE_MS</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a4791d2b56447d04ed38f856604ed53ad</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_FRAME_SPACING_MS</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ad3df373183a99f0161894012e3c251b1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_RESET_TIME_US</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a5e46e8f15b9ba2ac3ac0935d11a778fc</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_SAFE_TIMEOUT</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a683981e7f037dd43d22e1e337db287bd</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_MUTEX_SAFE_TIMEOUT</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a1c080ccbe5c2b8e9c7229f3c10f74a8f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_QUEUE_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a60c0e1e27fef536d95040e98b856975a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_QUEUE_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a579e6003d89168493306bca5848f301a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BUTTON_QUEUE_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a6ca45550c06013af8c3cec978a7271ab</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>UART_QUEUE_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a1e15a8ac2f944de5c675aa81c1c653c4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GAME_QUEUE_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a28eeb53d2d0b05fd6f19dd85f101b40e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANIMATION_QUEUE_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>affde4e66cef922d3fa060b3013d10c3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SCREEN_SAVER_QUEUE_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>afd353feba5d0f1707fb7a70438c4dc1f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>WEB_SERVER_QUEUE_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ac550726b2b0adb0539e9dc4f8a1be108</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_TASK_STACK_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a7b127f4562618fb23c61f79d2a6a2201</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_TASK_STACK_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a71a39cddac7f169064d88214a759deb8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BUTTON_TASK_STACK_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>adbb9d0dfbf98763aa25c6a52814f0bf8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>UART_TASK_STACK_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a1a9da456d063076e469758050a0a472f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GAME_TASK_STACK_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a3742ac4e005f9654c81dac1b035e8a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANIMATION_TASK_STACK_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a6e9d80d372a5d67bfe61ef2373242891</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SCREEN_SAVER_TASK_STACK_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a203c5698d5ed09c06220f3f660867903</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>TEST_TASK_STACK_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a5a50c304bdd9e66032df79cfea02fb35</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>WEB_SERVER_TASK_STACK_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ad02ca6747c039790bae842a9574ce45b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RESET_BUTTON_TASK_STACK_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>aa89cf203c391fae0636372b71659d297</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PROMOTION_BUTTON_TASK_STACK_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a0777cbb816d50dcb813a9bf1b98dd46e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_LIGHT_TASK_STACK_SIZE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ad527fcf9ba093641832abb9056b25774</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_TASK_PRIORITY</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a929063c46e63eb78271df5b5fd5819e9</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_TASK_PRIORITY</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ab8ed4eae602e4b2be89befb487b83d26</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BUTTON_TASK_PRIORITY</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a94dea431a1c059625b45f816ab129043</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>UART_TASK_PRIORITY</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a57dcc29bb266003c63803b06ea25b26d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GAME_TASK_PRIORITY</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a006aed1ec383676054e95d995acd427b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANIMATION_TASK_PRIORITY</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>aa535c253da7bfeb4f3fcba9b47fcfe4f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SCREEN_SAVER_TASK_PRIORITY</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a66161fe4e247e0f129ebe3b2bf3d1d42</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>TEST_TASK_PRIORITY</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a93921d3292005836714afe15fad7d078</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>WEB_SERVER_TASK_PRIORITY</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a21c1cebc5db393a9f06dfb6266a65241</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RESET_BUTTON_TASK_PRIORITY</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a9317390fd5035b22532eaaa19884d747</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PROMOTION_BUTTON_TASK_PRIORITY</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a58d022d18b73ee9efae31b39b6fd4343</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_LIGHT_TASK_PRIORITY</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ad420cbd4164a1c35acf8ef56810955ab</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SAFE_CREATE_QUEUE</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a4821223bcff83669f6b0414b90542738</anchor>
      <arglist>(handle, size, item_size, name)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SAFE_CREATE_MUTEX</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a9ca3a534387caddce17a0aa3afb7a332</anchor>
      <arglist>(handle, name)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_system_init</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ae3398aa451de52fedd3ac9e5830fca76</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_memory_systems_init</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ac2f54e3f77005d32b1df12eca333ed3e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_hardware_init</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a41db379171eabe6fccb60019e0e60549</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_create_queues</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a918837616a81eb433243f2c1e109f73a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_create_mutexes</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a9bb91e5162ab07c0d6e18df6a5a9f43e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_create_timers</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ad4a9b6f60cb5280918a47885121ea229</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_start_timers</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a315226a40faf8a392b6328ed7c26340f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>button_scan_timer_callback</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a60ba01ed8c95cf7326c889ea1b6a2bc2</anchor>
      <arglist>(TimerHandle_t xTimer)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_scan_timer_callback</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a904cb90015996d3ad0532e71bc502318</anchor>
      <arglist>(TimerHandle_t xTimer)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_gpio_init</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>af8e4ec2ba6554d39c4f7c9574718efdc</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_uart_send_string</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a7fb9ad5c97d8450a447d81ae4c6199fe</anchor>
      <arglist>(const char *str)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_uart_printf</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>af0a70e7b77c14224b0efc66be5c59396</anchor>
      <arglist>(const char *format,...)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_led_set_pixel</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a6f66538255c01c03931e08eac42e6e8f</anchor>
      <arglist>(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_led_set_all</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>aba042f04f5d5fcb5bfb8f1ea039be359</anchor>
      <arglist>(uint8_t red, uint8_t green, uint8_t blue)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_matrix_get_status</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a8472ef30fed91b8fa2c1efc57542ec33</anchor>
      <arglist>(uint8_t *status)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>chess_print_system_info</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>acc6c5ec2b59b331d0d27ef56a9f0c462</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>chess_monitor_tasks</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a0321f7ecdafb014dbfb05309bf810493</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>is_demo_mode_enabled</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ad190bb08e29e63b6f04a3715dae88706</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>matrix_event_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a1b93ff1badbca8962927cb6fd837463e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>matrix_command_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>abc01112e6afa1b3e029218e52b7c2196</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>matrix_response_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a9b3fea6e381f159f9e088382e4040d27</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>button_event_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a05107bbe85fd87a92dad9efd71dbd653</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>button_command_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a5f1958b9037083cf953f250fe1f364a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>uart_command_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a7e436e9dcca9a044bb9264755b4caa9e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>uart_response_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a02a776b41d5e50969e5cf68dcbbc2c69</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>game_command_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a4ed770cbe46604586e494bae84b62304</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>game_status_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a6dfc9956f36db9a85a34268c6e14b059</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>animation_command_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a6fa8af7d1c0b048b9f2c363a9fb2b288</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>animation_status_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ad2c6aca6dc859def2315ed1c26f11aad</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>screen_saver_command_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ad6e2650c9733357ca904426081ccee72</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>screen_saver_status_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a494d670fc3ed5c9bf7fc74a908a92712</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>matter_command_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a1f265b8287aba27035f07a200c7ddfec</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>matter_status_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a17f6bae025bf70b7775d1dd5e880e909</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>web_command_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a642abc1652d446215a18d55b0517edba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>web_server_command_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>aaed4d7c5bc3e8140853498b965ec94e2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>web_server_status_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>aa17b405ee779d94d3b6954dd83712591</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>test_command_queue</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a849df7d33a2eeeb8383dab84223a7813</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SemaphoreHandle_t</type>
      <name>led_mutex</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a864f0cc9b1f2e30bd3c0e225e2c48552</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SemaphoreHandle_t</type>
      <name>matrix_mutex</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a80a74d10dccdaa933010d0e5cf7202d8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SemaphoreHandle_t</type>
      <name>button_mutex</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a17744dc0b4a21889e7e9527835edc068</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SemaphoreHandle_t</type>
      <name>game_mutex</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>af5298ceb88597b4b25328ac7e1af7ff0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SemaphoreHandle_t</type>
      <name>system_mutex</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a54b3156204f85b12fbf1a9c012730afc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SemaphoreHandle_t</type>
      <name>uart_mutex</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a8eca84d3e2c7c0adf364819e74b1bd9b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TimerHandle_t</type>
      <name>matrix_scan_timer</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ad589ad22574a5fdb69645141759216c4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TimerHandle_t</type>
      <name>button_scan_timer</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>af58f98e02d7d24dcc49a348ea1f22e77</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TimerHandle_t</type>
      <name>system_health_timer</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a2d90fd6adb05ddb54ae494a47e311309</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const gpio_num_t</type>
      <name>matrix_row_pins</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>ad9e3ceb44aa3ff2deb6b45b6079cc7e8</anchor>
      <arglist>[8]</arglist>
    </member>
    <member kind="variable">
      <type>const gpio_num_t</type>
      <name>matrix_col_pins</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a2d07c58a77430cba050b46dd496ce081</anchor>
      <arglist>[8]</arglist>
    </member>
    <member kind="variable">
      <type>const gpio_num_t</type>
      <name>promotion_button_pins_a</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>aa13f5879e99176ffada8295c49c67b6c</anchor>
      <arglist>[4]</arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>led_task_handle</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a6cf81c896b094e20ff5752ee1133a8f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>matrix_task_handle</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a9de1b87e0b6692c90f1c5cfd9e13d444</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>button_task_handle</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>aba5c2db71f9fa5cd9f3e506e91ffe6d3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>uart_task_handle</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a4e4967ac70f1e8a89b2fea62027a968c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>game_task_handle</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>acb49a688d90460dc48f7e0b976ace62d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>animation_task_handle</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a2a20898222a990efbce3e4239f2d9aee</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>screen_saver_task_handle</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a1b8705c6542b99f74f769212463bed0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>test_task_handle</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>abafefd1ba465b609ca39fd6fc502095b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>matter_task_handle</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a3003870ab7d0f1243695d9f94cee10f9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>web_server_task_handle</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a36e9302901a63037025cd7970fa8adbe</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>reset_button_task_handle</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>aeb8dd1b563b914d04c90f377454ad032</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>promotion_button_task_handle</name>
      <anchorfile>freertos__chess_8h.html</anchorfile>
      <anchor>a2cf065fd2e74d6ad91b3da384a96c60b</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>led_mapping.h</name>
    <path>components/freertos_chess/include/</path>
    <filename>led__mapping_8h.html</filename>
    <member kind="function">
      <type>uint8_t</type>
      <name>chess_pos_to_led_index</name>
      <anchorfile>led__mapping_8h.html</anchorfile>
      <anchor>a6f6c321f16b2494986ebfb82fa9bf5df</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_index_to_chess_pos</name>
      <anchorfile>led__mapping_8h.html</anchorfile>
      <anchor>afa2f63d0405f6da06cdeceee60aaa9d7</anchor>
      <arglist>(uint8_t led_index, uint8_t *row, uint8_t *col)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>chess_notation_to_led_index</name>
      <anchorfile>led__mapping_8h.html</anchorfile>
      <anchor>ac190470b2f78837d3df987b0951264dd</anchor>
      <arglist>(const char *notation)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_led_mapping</name>
      <anchorfile>led__mapping_8h.html</anchorfile>
      <anchor>a8fdf823e1e4db3abca2c962afc09f826</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>shared_buffer_pool.h</name>
    <path>components/freertos_chess/include/</path>
    <filename>shared__buffer__pool_8h.html</filename>
    <class kind="struct">buffer_pool_stats_t</class>
    <member kind="define">
      <type>#define</type>
      <name>get_shared_buffer</name>
      <anchorfile>shared__buffer__pool_8h.html</anchorfile>
      <anchor>a70318c0bb964a91191f81e8b06a264eb</anchor>
      <arglist>(size)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SAFE_GET_BUFFER</name>
      <anchorfile>shared__buffer__pool_8h.html</anchorfile>
      <anchor>a4fd36e7fb9f9dff5eaa936115b4a5e48</anchor>
      <arglist>(ptr, size, cleanup_label)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SAFE_RELEASE_BUFFER</name>
      <anchorfile>shared__buffer__pool_8h.html</anchorfile>
      <anchor>ab1e2b3bd6e04fde2a413dd3dae10c415</anchor>
      <arglist>(ptr)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>buffer_pool_init</name>
      <anchorfile>shared__buffer__pool_8h.html</anchorfile>
      <anchor>a57e203054eac9c4b6834783c64844d9b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>buffer_pool_deinit</name>
      <anchorfile>shared__buffer__pool_8h.html</anchorfile>
      <anchor>ab7941cd3b2567eec4538fb234680a9bf</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>char *</type>
      <name>get_shared_buffer_debug</name>
      <anchorfile>shared__buffer__pool_8h.html</anchorfile>
      <anchor>a985626f60ba733d611f238158cd15aac</anchor>
      <arglist>(size_t min_size, const char *file, int line)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>release_shared_buffer</name>
      <anchorfile>shared__buffer__pool_8h.html</anchorfile>
      <anchor>a2dcea7230ff8c3509b7b7adaeba9874e</anchor>
      <arglist>(char *buffer)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>buffer_pool_print_status</name>
      <anchorfile>shared__buffer__pool_8h.html</anchorfile>
      <anchor>a78258eeac6ef7d0f8d2cf999f0605b0c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>buffer_pool_stats_t</type>
      <name>buffer_pool_get_stats</name>
      <anchorfile>shared__buffer__pool_8h.html</anchorfile>
      <anchor>a8d1141e7eeb3bedad5c0d1ad575a953d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>buffer_pool_is_healthy</name>
      <anchorfile>shared__buffer__pool_8h.html</anchorfile>
      <anchor>acd58d78b420eb7bbeb83dd9b2d249be4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>buffer_pool_detect_leaks</name>
      <anchorfile>shared__buffer__pool_8h.html</anchorfile>
      <anchor>a212c8f17c9792d37e2c884ef14ed8e69</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>streaming_output.h</name>
    <path>components/freertos_chess/include/</path>
    <filename>streaming__output_8h.html</filename>
    <class kind="struct">streaming_output_t</class>
    <class kind="struct">streaming_stats_t</class>
    <member kind="define">
      <type>#define</type>
      <name>STREAM_LINE_BUFFER_SIZE</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a23a8466caaf2282acdc686a4a9dde41d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STREAM_MAX_OUTPUT_TARGETS</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a69dfd27db3e015fc85f81dea4affbbeb</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STREAM_CHESS_BOARD</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>ad0fce4a1af286f625bf5aa844e794cb2</anchor>
      <arglist>(board_array)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STREAM_LED_BOARD</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a51d51314482adade900fa2659aede2b5</anchor>
      <arglist>(led_array)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STREAM_CHUNKED_REPORT</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a6c724a82f7d007d4bffa449079138389</anchor>
      <arglist>(title)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STREAM_REPORT_END</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a8caa346fb405cea674904eee2b84abad</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>stream_type_t</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>aebb40016c198e3e5e1f82c86929dc454</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STREAM_UART</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>aebb40016c198e3e5e1f82c86929dc454a041393ad29ad01be710ecd3d8d77c2b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STREAM_WEB</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>aebb40016c198e3e5e1f82c86929dc454ae9b3c32d282a15b603c991744840756b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STREAM_QUEUE</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>aebb40016c198e3e5e1f82c86929dc454ab09258d1ed838a3032df63357bc7d4b7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>stream_line_ending_t</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>affdaded1b69ffceddaa030802cd994a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STREAM_LF</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>affdaded1b69ffceddaa030802cd994a8aabee12901eb5b407a8ed908c56f14c88</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STREAM_CRLF</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>affdaded1b69ffceddaa030802cd994a8a54fc8c6ca2040ea2b614ef90ae36a068</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>streaming_output_init</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a5ce23c8be43f17f9b75c37d63fdb7eac</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>streaming_output_deinit</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a936645e3895fd2cd3cda93e2b9302a4d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>streaming_set_uart_output</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a8aa67921edc9b4e0c2ffcf2660a96982</anchor>
      <arglist>(int uart_port)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>streaming_set_web_output</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>aae842b19f1975942c5b3674cb42fbddb</anchor>
      <arglist>(void *web_client)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>streaming_set_queue_output</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>aca427711530c81651095df3f70106d66</anchor>
      <arglist>(QueueHandle_t queue)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_printf</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a19603309bb16a4e39d500f27a16101c1</anchor>
      <arglist>(const char *format,...) __attribute__((format(printf</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t esp_err_t</type>
      <name>stream_write</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a70b4216dd32edbb9b820bb46e0c6b45b</anchor>
      <arglist>(const char *data, size_t len)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_writeln</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a81bb5955edffac0083f39e3114c99386</anchor>
      <arglist>(const char *data)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_flush</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>aee33cf86bf19206f11756f9e94544b6d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_set_auto_flush</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a1435caf264b122a887df306e6b1b9d11</anchor>
      <arglist>(bool enabled)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_set_line_ending</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a9173e4c76c40d5e2c7aeb6e63bd4cb31</anchor>
      <arglist>(stream_line_ending_t ending)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_board_header</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a59f0895b69aa9d69a3386adbc8f6c6e5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_board_row</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>aeee82161b919e7c2993a0a40b564cf9f</anchor>
      <arglist>(int row, const char *pieces)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_board_footer</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>ab702b0aa249cc6463ecd01db54bea295</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_led_board_header</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a2bf893bcdd558c5d065c5b2dba1b369c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_led_board_row</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a812ce05742f894755d8b5d0b14019776</anchor>
      <arglist>(int row, const uint32_t *led_colors)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>streaming_print_stats</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a2be8864cdaed57fd8249185a4ade538f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>streaming_stats_t</type>
      <name>streaming_get_stats</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>aad4c641177bba008c8ec5c0a0265ac7c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>streaming_reset_stats</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>acf5e7d713d834d6fb513201f5f761582</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>streaming_is_healthy</name>
      <anchorfile>streaming__output_8h.html</anchorfile>
      <anchor>a251a50a8653a9c02de813b176cfa1718</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>led_mapping.c</name>
    <path>components/freertos_chess/</path>
    <filename>led__mapping_8c.html</filename>
    <includes id="led__mapping_8h" name="led_mapping.h" local="yes" import="no" module="no" objc="no">led_mapping.h</includes>
    <member kind="function">
      <type>uint8_t</type>
      <name>chess_pos_to_led_index</name>
      <anchorfile>led__mapping_8c.html</anchorfile>
      <anchor>a6f6c321f16b2494986ebfb82fa9bf5df</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_index_to_chess_pos</name>
      <anchorfile>led__mapping_8c.html</anchorfile>
      <anchor>afa2f63d0405f6da06cdeceee60aaa9d7</anchor>
      <arglist>(uint8_t led_index, uint8_t *row, uint8_t *col)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>chess_notation_to_led_index</name>
      <anchorfile>led__mapping_8c.html</anchorfile>
      <anchor>ac190470b2f78837d3df987b0951264dd</anchor>
      <arglist>(const char *notation)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_led_mapping</name>
      <anchorfile>led__mapping_8c.html</anchorfile>
      <anchor>a8fdf823e1e4db3abca2c962afc09f826</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>led__mapping_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>shared_buffer_pool.c</name>
    <path>components/freertos_chess/</path>
    <filename>shared__buffer__pool_8c.html</filename>
    <includes id="shared__buffer__pool_8h" name="shared_buffer_pool.h" local="yes" import="no" module="no" objc="no">shared_buffer_pool.h</includes>
    <class kind="struct">shared_buffer_t</class>
    <member kind="define">
      <type>#define</type>
      <name>BUFFER_POOL_SIZE</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>ac1ae1af79e723ac8e3093efca7af3e71</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BUFFER_SIZE</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>a6b20d41d6252e9871430c242cb1a56e7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX_BUFFER_WAIT_MS</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>abc20ae84b1a406e3569c2ffe4fb8386a</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>buffer_pool_init</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>a57e203054eac9c4b6834783c64844d9b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>buffer_pool_deinit</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>ab7941cd3b2567eec4538fb234680a9bf</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>char *</type>
      <name>get_shared_buffer_debug</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>a985626f60ba733d611f238158cd15aac</anchor>
      <arglist>(size_t min_size, const char *file, int line)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>release_shared_buffer</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>a2dcea7230ff8c3509b7b7adaeba9874e</anchor>
      <arglist>(char *buffer)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>buffer_pool_print_status</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>a78258eeac6ef7d0f8d2cf999f0605b0c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>buffer_pool_stats_t</type>
      <name>buffer_pool_get_stats</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>a8d1141e7eeb3bedad5c0d1ad575a953d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>buffer_pool_is_healthy</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>acd58d78b420eb7bbeb83dd9b2d249be4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>buffer_pool_detect_leaks</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>a212c8f17c9792d37e2c884ef14ed8e69</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static shared_buffer_t</type>
      <name>buffer_pool</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>a3a69a0ca798cdd0bfa8f5ce01a6ae34e</anchor>
      <arglist>[4]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static SemaphoreHandle_t</type>
      <name>buffer_pool_mutex</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>adbfe8616935903bcea16858a74066ec6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>pool_initialized</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>a5c84208b4e9d308124c7e5f9097eacd0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>total_allocations</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>a1a1069d01f9024724c8d8931a96be0e7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>total_releases</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>a6a805dab9e7fcd9501a2f7059feedea1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>peak_usage</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>a82e071b75d18efa8bb95688becb3cbcd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>current_usage</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>a21d71597e5fe978f0a63b0374bc50eef</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>allocation_failures</name>
      <anchorfile>shared__buffer__pool_8c.html</anchorfile>
      <anchor>acd1fec8326abc9b532e4004cd0283e88</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>streaming_output.c</name>
    <path>components/freertos_chess/</path>
    <filename>streaming__output_8c.html</filename>
    <includes id="streaming__output_8h" name="streaming_output.h" local="yes" import="no" module="no" objc="no">streaming_output.h</includes>
    <includes id="chess__types_8h" name="chess_types.h" local="yes" import="no" module="no" objc="no">chess_types.h</includes>
    <includes id="led__mapping_8h" name="led_mapping.h" local="yes" import="no" module="no" objc="no">led_mapping.h</includes>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>stream_write_uart</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a1952cdd16ea597f54d90ff99e53612f6</anchor>
      <arglist>(const char *data, size_t len)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>stream_write_web</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a230e6d355273f51e78c3de12e22c7736</anchor>
      <arglist>(const char *data, size_t len)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>stream_write_queue</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a0e07c8570098aa75ba738a5f7c361080</anchor>
      <arglist>(const char *data, size_t len)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>streaming_output_init</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a5ce23c8be43f17f9b75c37d63fdb7eac</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>streaming_output_deinit</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a936645e3895fd2cd3cda93e2b9302a4d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>streaming_set_uart_output</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a8aa67921edc9b4e0c2ffcf2660a96982</anchor>
      <arglist>(int uart_port)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>streaming_set_web_output</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>aae842b19f1975942c5b3674cb42fbddb</anchor>
      <arglist>(void *web_client)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>streaming_set_queue_output</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>aca427711530c81651095df3f70106d66</anchor>
      <arglist>(QueueHandle_t queue)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_printf</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a951feaae516472ce834f088013ea4bc4</anchor>
      <arglist>(const char *format,...)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_write</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a2c7a31a7ff9caa7a3f2d9d84b1747377</anchor>
      <arglist>(const char *data, size_t len)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_writeln</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a81bb5955edffac0083f39e3114c99386</anchor>
      <arglist>(const char *data)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_flush</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>aee33cf86bf19206f11756f9e94544b6d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_set_auto_flush</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a1435caf264b122a887df306e6b1b9d11</anchor>
      <arglist>(bool enabled)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_set_line_ending</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a9173e4c76c40d5e2c7aeb6e63bd4cb31</anchor>
      <arglist>(stream_line_ending_t ending)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_board_header</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a59f0895b69aa9d69a3386adbc8f6c6e5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_board_row</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>aeee82161b919e7c2993a0a40b564cf9f</anchor>
      <arglist>(int row, const char *pieces)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_board_footer</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>ab702b0aa249cc6463ecd01db54bea295</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_led_board_header</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a2bf893bcdd558c5d065c5b2dba1b369c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stream_led_board_row</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a812ce05742f894755d8b5d0b14019776</anchor>
      <arglist>(int row, const uint32_t *led_colors)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>streaming_print_stats</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a2be8864cdaed57fd8249185a4ade538f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>streaming_stats_t</type>
      <name>streaming_get_stats</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>aad4c641177bba008c8ec5c0a0265ac7c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>streaming_reset_stats</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>acf5e7d713d834d6fb513201f5f761582</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>streaming_is_healthy</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a251a50a8653a9c02de813b176cfa1718</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static SemaphoreHandle_t</type>
      <name>streaming_mutex</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>aa8255bd6562ac594bf8c271b0af18629</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>system_initialized</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>ab2336e816f0a31f0e3e85fba396e4340</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static streaming_stats_t</type>
      <name>stats</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>acdc2beab71d64ed93f6bf73699e3a8f9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static streaming_output_t</type>
      <name>current_output</name>
      <anchorfile>streaming__output_8c.html</anchorfile>
      <anchor>af1926ad29634d13b1230a971167e3277</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>game_led_animations.c</name>
    <path>components/game_led_animations/</path>
    <filename>game__led__animations_8c.html</filename>
    <includes id="game__led__animations_8h" name="game_led_animations.h" local="yes" import="no" module="no" objc="no">game_led_animations.h</includes>
    <includes id="led__mapping_8h" name="led_mapping.h" local="yes" import="no" module="no" objc="no">led_mapping.h</includes>
    <includes id="led__task__simple_8h" name="led_task_simple.h" local="yes" import="no" module="no" objc="no">led_task_simple.h</includes>
    <member kind="function">
      <type></type>
      <name>__attribute__</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a36f229bbda63818246ec86181b1fdd42</anchor>
      <arglist>((unused))</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>apply_color_safe</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>ae0c1dab4ee7a18e66fc96b45fe59e0c6</anchor>
      <arglist>(uint8_t led_index, rgb_color_t color)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static float</type>
      <name>get_board_distance</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a4dd115ab709bac86fcd837af7bc2c791</anchor>
      <arglist>(uint8_t pos1, uint8_t pos2)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>endgame_animation_victory_wave</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a30cf1a62ddf72a59e47db6c5a7d16bd3</anchor>
      <arglist>(uint32_t frame)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>endgame_animation_victory_circles</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a769b87cd1cca43fe03922727d6b1378b</anchor>
      <arglist>(uint32_t frame)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>endgame_animation_victory_cascade</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a4f47ddefd68242ce100d748cc7493aff</anchor>
      <arglist>(uint32_t frame)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>endgame_animation_victory_fireworks</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>ab31da73e7fa32113a7c05f187bb9468f</anchor>
      <arglist>(uint32_t frame)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>endgame_animation_victory_crown</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a9ca82a56a74ac5dc7ffa31f24b5d57fe</anchor>
      <arglist>(uint32_t frame)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>start_subtle_piece_animation</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>ac3099afed7bffbea6eaba00603a015a9</anchor>
      <arglist>(uint8_t piece_position, subtle_anim_type_t anim_type)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>start_subtle_button_animation</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>abb3b14cd5eb31a70450c0a404902e4e0</anchor>
      <arglist>(uint8_t button_id, subtle_anim_type_t anim_type)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>apply_subtle_animation</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a5fb652dd2b2cbe7cdac3b9cbaa30763c</anchor>
      <arglist>(uint8_t led_index, subtle_animation_state_t *anim)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>subtle_animation_timer_callback</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>aea794bf814cff2d03ccd30530878876f</anchor>
      <arglist>(TimerHandle_t timer)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>animation_timer_callback</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>af4170c2454c1c13a7c40fecb32c032d3</anchor>
      <arglist>(TimerHandle_t timer)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_led_animations_init</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a9c35ac742c83bf78240d93599dd54556</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>start_endgame_animation</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a512b0ba5745a638b00545a16423c4e17</anchor>
      <arglist>(endgame_animation_type_t animation_type, uint8_t king_position)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stop_endgame_animation</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>ad8316144bc70b897342c47d341b52b5b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>is_endgame_animation_running</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>abc332d95d86dbce7893f640161167edb</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>get_endgame_animation_name</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a657e1bb5d40ff05278f1fd817295a34b</anchor>
      <arglist>(endgame_animation_type_t animation_type)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stop_all_subtle_animations</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a7d2e178a6334fe293dd266e15b194ceb</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>animation_system_active</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a613683b2a29d20556b1ad11c52bdfdb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>endgame_animation_running</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a0ff12f065a2e5fbd5d81bff9fd425cbf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static endgame_animation_type_t</type>
      <name>current_endgame_animation</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a76483b95c5cbeaec7b68b61fdba3e1b0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>winning_king_position</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>abb6170c1df734838bc417eff360db75a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static TimerHandle_t</type>
      <name>animation_timer</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a43f6b3d51a255773c25e705a8df30eb2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static TimerHandle_t</type>
      <name>subtle_animation_timer</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>ace720982d876787deb18d63449924dce</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static wave_animation_state_t</type>
      <name>wave_state</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>af779e4140456fa6d1f3451df49addddc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static subtle_animation_state_t</type>
      <name>subtle_pieces</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a5f7ea887a638f622964d93b2786353c5</anchor>
      <arglist>[64]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static subtle_animation_state_t</type>
      <name>subtle_buttons</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a3d474a43fe7077af5724dcc84d18ec10</anchor>
      <arglist>[9]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const rgb_color_t</type>
      <name>COLOR_RED</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a867cddabd2cfa07bb13afb8529f29378</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const rgb_color_t</type>
      <name>COLOR_GREEN</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a9439369a6a8cbd157e2f327e3cb118ed</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const rgb_color_t</type>
      <name>COLOR_BLUE</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a061e154b37b6f87ae92477358995ba95</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const rgb_color_t</type>
      <name>COLOR_YELLOW</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>ad35904f84034100fc638eba6afaa39a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const rgb_color_t</type>
      <name>COLOR_ORANGE</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a8ddfd053713152224b52e3ed2267be88</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const rgb_color_t</type>
      <name>COLOR_PURPLE</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>afdb7e8b9694b6a7dcfb76fb9bef5b572</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const rgb_color_t</type>
      <name>COLOR_WHITE</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>ab4c3aaf13591bf876d0c3699c4eacca0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const rgb_color_t</type>
      <name>COLOR_GOLD</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a4216ddd7650cfba7cc15973e9509576c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const rgb_color_t</type>
      <name>wave_blue_palette</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a564b17e3bc280df01e678c047bd222a1</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const rgb_color_t</type>
      <name>enemy_red_palette</name>
      <anchorfile>game__led__animations_8c.html</anchorfile>
      <anchor>a06640a5d84f366f00c51139f93704f21</anchor>
      <arglist>[]</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>game_led_animations.h</name>
    <path>components/game_led_animations/include/</path>
    <filename>game__led__animations_8h.html</filename>
    <class kind="struct">rgb_color_t</class>
    <class kind="struct">wave_t</class>
    <class kind="struct">wave_animation_state_t</class>
    <class kind="struct">firework_t</class>
    <class kind="struct">subtle_animation_state_t</class>
    <member kind="define">
      <type>#define</type>
      <name>MAX_WAVES</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>a1767d604401a9b047932166494ed7b9e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX_FIREWORKS</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>ad146c422c93828b517dc1801a40ca01f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>init_endgame_animation_system</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>aff587ca038d004104b274391cb221113</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>endgame_animation_type_t</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>a30603ef39ed3a32a5ae1be1274874159</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_ANIM_VICTORY_WAVE</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>a30603ef39ed3a32a5ae1be1274874159a7559cb124fcc149b6ba6375635c70757</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_ANIM_VICTORY_CIRCLES</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>a30603ef39ed3a32a5ae1be1274874159a79de3a208cf30fd7a25d2232fdcdfde6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_ANIM_VICTORY_CASCADE</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>a30603ef39ed3a32a5ae1be1274874159a25765c0de51d15b25fa32fb63a71a415</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_ANIM_VICTORY_FIREWORKS</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>a30603ef39ed3a32a5ae1be1274874159a53c907e548a6c685bad62bfe17a88f37</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_ANIM_VICTORY_CROWN</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>a30603ef39ed3a32a5ae1be1274874159a2817775da3ceef1a44819ed7264c0b99</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_ANIM_MAX</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>a30603ef39ed3a32a5ae1be1274874159a2e296498d5b85c7d2d71fbd804fc78d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>subtle_anim_type_t</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>aaf8aea34899d30734001c2e6a651bc08</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SUBTLE_ANIM_GENTLE_WAVE</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>aaf8aea34899d30734001c2e6a651bc08ad9382dfe9593aaba6deae21d6cac0a03</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SUBTLE_ANIM_WARM_GLOW</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>aaf8aea34899d30734001c2e6a651bc08abfaa1ef48fd45abad603d05144e2d0e0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SUBTLE_ANIM_COOL_PULSE</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>aaf8aea34899d30734001c2e6a651bc08aeb7102f738091f89240283cb70a787e4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SUBTLE_ANIM_WHITE_WINS</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>aaf8aea34899d30734001c2e6a651bc08ae03d9ddc78ae7a6b61ab70c35b0f0dcf</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SUBTLE_ANIM_BLACK_WINS</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>aaf8aea34899d30734001c2e6a651bc08ae5bf0eff905cbae51e1700277a3d2fb3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SUBTLE_ANIM_DRAW</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>aaf8aea34899d30734001c2e6a651bc08aa698d92cab2634b768d1062dfb68e61f</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_led_animations_init</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>a9c35ac742c83bf78240d93599dd54556</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>start_endgame_animation</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>a512b0ba5745a638b00545a16423c4e17</anchor>
      <arglist>(endgame_animation_type_t animation_type, uint8_t king_position)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stop_endgame_animation</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>ad8316144bc70b897342c47d341b52b5b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>is_endgame_animation_running</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>abc332d95d86dbce7893f640161167edb</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>get_endgame_animation_name</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>a657e1bb5d40ff05278f1fd817295a34b</anchor>
      <arglist>(endgame_animation_type_t animation_type)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>start_subtle_piece_animation</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>ac3099afed7bffbea6eaba00603a015a9</anchor>
      <arglist>(uint8_t piece_position, subtle_anim_type_t anim_type)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>start_subtle_button_animation</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>abb3b14cd5eb31a70450c0a404902e4e0</anchor>
      <arglist>(uint8_t button_id, subtle_anim_type_t anim_type)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>stop_all_subtle_animations</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>a7d2e178a6334fe293dd266e15b194ceb</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>activate_subtle_animations_for_movable_pieces</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>acb38fb824c92dc6d289f623bc15fcac6</anchor>
      <arglist>(uint8_t *movable_positions, uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>activate_subtle_animations_for_buttons</name>
      <anchorfile>game__led__animations_8h.html</anchorfile>
      <anchor>ae9a2e770a4936e32e42c67ebb236ce01</anchor>
      <arglist>(uint8_t *available_buttons, uint8_t count)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>demo_mode_helpers.c</name>
    <path>components/game_task/</path>
    <filename>demo__mode__helpers_8c.html</filename>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>demo_is_castling_move</name>
      <anchorfile>demo__mode__helpers_8c.html</anchorfile>
      <anchor>ab5480af4a47935696e63f5cb45a754cc</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col, piece_t board[8][8])</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>demo_get_castling_rook_squares</name>
      <anchorfile>demo__mode__helpers_8c.html</anchorfile>
      <anchor>a1e81ab60269dcc2083cbe4acf50b3cbf</anchor>
      <arglist>(uint8_t king_from_row, uint8_t king_from_col, uint8_t king_to_col, uint8_t *rook_from_row, uint8_t *rook_from_col, uint8_t *rook_to_row, uint8_t *rook_to_col)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>demo_is_promotion_move</name>
      <anchorfile>demo__mode__helpers_8c.html</anchorfile>
      <anchor>a765d58ac494ce1812d1a714d714915b4</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, piece_t board[8][8])</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>demo_ensure_safe_timing</name>
      <anchorfile>demo__mode__helpers_8c.html</anchorfile>
      <anchor>a610e6026c21231070bf571b79be34430</anchor>
      <arglist>(uint32_t *last_move_time_ms, uint32_t interval_ms)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>game_led_direct.c</name>
    <path>components/game_task/</path>
    <filename>game__led__direct_8c.html</filename>
    <includes id="game__task_8h" name="game_task.h" local="yes" import="no" module="no" objc="no">game_task.h</includes>
    <includes id="led__task_8h" name="led_task.h" local="yes" import="no" module="no" objc="no">led_task.h</includes>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <includes id="chess__types_8h" name="chess_types.h" local="yes" import="no" module="no" objc="no">chess_types.h</includes>
    <includes id="game__led__animations_8h" name="game_led_animations.h" local="yes" import="no" module="no" objc="no">game_led_animations.h</includes>
    <includes id="led__mapping_8h" name="led_mapping.h" local="yes" import="no" module="no" objc="no">led_mapping.h</includes>
    <member kind="function">
      <type>void</type>
      <name>game_show_move_direct</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>a65d45d8068e75012a3bf91d813a86993</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_piece_lift_direct</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>aad4c6f489504e1aace2045420bcec3d1</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_valid_moves_direct</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>af337fa678e02039015341d7fa45e3ccc</anchor>
      <arglist>(uint8_t *valid_positions, uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_error_direct</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>a11c32da99485731ac1e0cae4f2174fcd</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_clear_highlights_direct</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>a8b43b89b88ce59ea25698ac5a8280bf2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_state_direct</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>a002b9dd58ec7cfbcac57367df453f258</anchor>
      <arglist>(piece_t *board)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_check_direct</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>a0ace822d82e2da6de18559787be957d0</anchor>
      <arglist>(uint8_t king_row, uint8_t king_col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_player_change_direct</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>abc7c8755788427711faa337e69d53e19</anchor>
      <arglist>(player_t current_player)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_invalid_move_error</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>a5f2645fc55410cfe671fe6e8f2a99f2f</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_button_error</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>a94723cd4e9d7f319e96c1d7aaa67fcf8</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_castling_guidance</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>a528221cd8d3c2dd6de0732883230033a</anchor>
      <arglist>(uint8_t king_row, uint8_t king_col, uint8_t rook_row, uint8_t rook_col, bool is_kingside)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_checkmate_direct</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>aea44edb838b9522e00dfd55da1856986</anchor>
      <arglist>(uint8_t king_row, uint8_t king_col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_promotion_direct</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>a47ce5497c44b6af58ed04cc1c3d806b8</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_castling_direct</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>aa112414da6cba72225e1b2958c958fa3</anchor>
      <arglist>(uint8_t king_from_row, uint8_t king_from_col, uint8_t king_to_row, uint8_t king_to_col, uint8_t rook_from_row, uint8_t rook_from_col, uint8_t rook_to_row, uint8_t rook_to_col)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>game__led__direct_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>game_task.c</name>
    <path>components/game_task/</path>
    <filename>game__task_8c.html</filename>
    <includes id="game__task_8h" name="game_task.h" local="yes" import="no" module="no" objc="no">game_task.h</includes>
    <includes id="button__task_8h" name="button_task.h" local="yes" import="no" module="no" objc="no">../button_task/include/button_task.h</includes>
    <includes id="chess__types_8h" name="chess_types.h" local="yes" import="no" module="no" objc="no">../freertos_chess/include/chess_types.h</includes>
    <includes id="streaming__output_8h" name="streaming_output.h" local="yes" import="no" module="no" objc="no">../freertos_chess/include/streaming_output.h</includes>
    <includes id="led__task_8h" name="led_task.h" local="yes" import="no" module="no" objc="no">../led_task/include/led_task.h</includes>
    <includes id="matrix__task_8h" name="matrix_task.h" local="yes" import="no" module="no" objc="no">../matrix_task/include/matrix_task.h</includes>
    <includes id="unified__animation__manager_8h" name="unified_animation_manager.h" local="yes" import="no" module="no" objc="no">../unified_animation_manager/include/unified_animation_manager.h</includes>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <includes id="game__led__animations_8h" name="game_led_animations.h" local="yes" import="no" module="no" objc="no">game_led_animations.h</includes>
    <includes id="led__mapping_8h" name="led_mapping.h" local="yes" import="no" module="no" objc="no">led_mapping.h</includes>
    <includes id="game__colors_8h" name="game_colors.h" local="yes" import="no" module="no" objc="no">game_colors.h</includes>
    <includes id="ha__light__task_8h" name="ha_light_task.h" local="yes" import="no" module="no" objc="no">../ha_light_task/include/ha_light_task.h</includes>
    <class kind="struct">castling_state_t</class>
    <class kind="struct">non_blocking_blink_state_t</class>
    <class kind="struct">king_resignation_state_t</class>
    <member kind="define">
      <type>#define</type>
      <name>MAX_MOVES_HISTORY</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a28d6040fd0cdaaa59aef59c0a4529ce6</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MOVE_VALIDATION_MS</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a2e965abc7c554be97017710630e9e0b7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>AUTO_NEW_GAME_CONFIRMATION_TIME_MS</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ab5d37ae881c4c3c8d9ac1a35e00e7b71</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX_CAPTURED_PIECES</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a22be28ff1955061a9bd8d64006fe6115</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX_ADVANTAGE_HISTORY</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a80792d272b6a0d629d67d2a0cf00f55a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_RESET</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a17f760256046df23dd0ab46602f04d02</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_BOLD</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a302607d6f585b7e9ba1c3eab8d2199d5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_BRIGHT_WHITE</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aa0977659d900017c4b65818c42a60a66</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_BRIGHT_RED</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a07544b84bb0b8d3c7b2580ca0b48a098</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_BRIGHT_GREEN</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a30dd0aa7ae6b1f5ffbcdd51024ee6c47</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_BRIGHT_YELLOW</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a27c03232867e0e1d6477d73e4e385026</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_BRIGHT_BLUE</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ae2eb7ceea22b4d7a5e8833f542bd7b24</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_BRIGHT_CYAN</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac45fac0dcefd97502824521c5f1d4c4d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BG_WHITE</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a6beded3f171517df3902c52f79f6fea2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BG_BLACK</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a0e5ebbe291d95cf1b8f1661252722fe0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BG_YELLOW</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ab31fe3e74b1137650d30ede5c9b86218</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>endgame_reason_t</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac2c530ea32ce9318e13d562eb6ed63ca</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_REASON_CHECKMATE</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac2c530ea32ce9318e13d562eb6ed63caa52cd39edd88b14727c7fad1cef6ee7d2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_REASON_CHECKMATE_EN_PASSANT</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac2c530ea32ce9318e13d562eb6ed63caa9103a2521e362b42cd1b9a8505075242</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_REASON_CHECKMATE_CASTLING</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac2c530ea32ce9318e13d562eb6ed63caa2b9fe119a75eb59424c83b9120108ac9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_REASON_CHECKMATE_PROMOTION</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac2c530ea32ce9318e13d562eb6ed63caa82859e0109b9e57299d8f20b6abf0801</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_REASON_CHECKMATE_DISCOVERED</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac2c530ea32ce9318e13d562eb6ed63caabde34381022074969ecde98b6f33583c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_REASON_STALEMATE</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac2c530ea32ce9318e13d562eb6ed63caa06fe60e50aa01b44d380419d81f1db15</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_REASON_50_MOVE</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac2c530ea32ce9318e13d562eb6ed63caa5808c364da6d0b918d480194b6b052de</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_REASON_REPETITION</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac2c530ea32ce9318e13d562eb6ed63caac48674036719b12ebdccbcaef0724ef8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_REASON_INSUFFICIENT</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac2c530ea32ce9318e13d562eb6ed63caa07cc538fbe0da7f3c53e2a65dc4cb764</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_REASON_TIMEOUT</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac2c530ea32ce9318e13d562eb6ed63caaf504ae744a15e3a3ec113ff53541df72</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ENDGAME_REASON_RESIGNATION</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac2c530ea32ce9318e13d562eb6ed63caa6572be1bd9ec4bf05582b9506e79f869</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>last_move_type_t</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a35ee807e9b8fc9f96e7a97f77e38b428</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LAST_MOVE_NORMAL</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a35ee807e9b8fc9f96e7a97f77e38b428adaa3371423259563b378fbafc75fd5d7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LAST_MOVE_EN_PASSANT</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a35ee807e9b8fc9f96e7a97f77e38b428a5e9afb17a5a3d533a10a72f8365c2fee</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LAST_MOVE_CASTLING</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a35ee807e9b8fc9f96e7a97f77e38b428a74b328b19fe88255418ae15211170d9e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LAST_MOVE_PROMOTION</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a35ee807e9b8fc9f96e7a97f77e38b428af03c0fabe13195d12257c4e19af68013</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LAST_MOVE_DISCOVERED</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a35ee807e9b8fc9f96e7a97f77e38b428a1f704b3fdc9edf5defa5ca3f23fef635</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>game_task_wdt_reset_safe</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ade55abfe5c56999016c2e1ec6b9583fa</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>resignation_main_timer_callback</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a40374b39b8cbe6fecea219144af3b3dd</anchor>
      <arglist>(TimerHandle_t xTimer)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>resignation_animation_timer_callback</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a71085e0827522509e5791df1cfa9f129</anchor>
      <arglist>(TimerHandle_t xTimer)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>resignation_start</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a2e8b9adc46a3a41117d46e39001f5e60</anchor>
      <arglist>(player_t player, uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>resignation_stop</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ae8b81fb2e3d4835fe8a0b74ac97abd53</anchor>
      <arglist>(bool finalize)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>resignation_update_button_leds</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a6b536ba6f8bb5ebf7a76124fbb8d9b10</anchor>
      <arglist>(uint32_t elapsed_ms)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>resignation_tick</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a3bf81752bb1e6c0587cfdb9804f883be</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>resignation_finalize_timeout</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5e2939300449fca26ad46d2a04e6ec08</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_highlight_opponent_pieces</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ad511756b40fd92878c0b2dde47feea60</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static char</type>
      <name>piece_to_char</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a18410c1d6d949a27fc7b6440dcf2dc72</anchor>
      <arglist>(piece_t piece)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static int8_t</type>
      <name>game_calculate_material_advantage</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>af3188c30ca0951b2b2e762ab26cb0a4b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>game_record_material_advantage</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>abd6d559fc6b46a6a06f830b5c5cec56a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>add_captured_piece</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a9f117ec973e2706358d99dfe843cd6ae</anchor>
      <arglist>(piece_t piece)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>game_check_promotion_needed</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a54aee9078e2583f3063e1a0baec25c21</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>game_update_promotion_anchor_led</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5b0ea1f9a91c492a93d4ad9b2afcdb59</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>game_process_promotion_button</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a6f86764d953efaad0ea951c27cea07e3</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>game_execute_promotion</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>af5345cde0988d26d0eb842aa7e33b202</anchor>
      <arglist>(promotion_choice_t choice)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_print_board_enhanced</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aee07708800ca9d9eee519b40b78f71d3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_execute_move_enhanced</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5ab63491e32394b086a3c916db195c31</anchor>
      <arglist>(chess_move_extended_t *move)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_generate_legal_moves</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a0108839d1688d03bc9eba3f1c581788e</anchor>
      <arglist>(player_t player)</arglist>
    </member>
    <member kind="function">
      <type>game_state_t</type>
      <name>game_analyze_position</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5d48aea6f905097dca8c53e1c2a057b1</anchor>
      <arglist>(player_t player)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_update_error_blink</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>accf34dd84b20310558e589194f84657a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_stop_error_blink</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a372a178141cd4d42682ec1b5a5f50e02</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_calculate_position_hash</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a2e80d005401f27a295b3f2b440d4bb88</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_position_repeated</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a009d7069912fb2689b7443575d3def38</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_add_position_to_history</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a7d059ee1bbc8427968feaf95af032112</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>game_calculate_material_balance</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a715422f9b34ce3aff72a852b7951ebba</anchor>
      <arglist>(int *white_material, int *black_material)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_get_material_string</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a13f42aca89279614ea9159dec44ae36c</anchor>
      <arglist>(char *buffer, size_t buffer_size)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>game_update_endgame_statistics</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a0edf3b817789dd7433c93714c7c2e0fe</anchor>
      <arglist>(game_result_type_t result_type)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>print_report_header</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a195ade6830068f1a49821f3d0e8f24db</anchor>
      <arglist>(const char *title)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>print_separator</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a1ec13300bc78171b6fcd9dd28e89ecb5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>print_report_line</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ae0a6f0bdd7eec3bac8e5f7c50d811214</anchor>
      <arglist>(const char *label, const char *fmt,...)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>game_print_endgame_report_uart</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aa4254801c2619ab854e6e41d640768bd</anchor>
      <arglist>(game_result_type_t result_type)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_white_wins</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac06b42b52fc2abf90b66dcbd73ecd4a0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_black_wins</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a4c5423c283319ea94779b79682ca9b98</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_draws</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a49a6874590b30a62f04f7ca6d3efcf05</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_total_games</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aed140b5d1ea696aa8fcd53aacc2303a9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>game_get_game_state_string</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a870a53ed0d150a3c293bcbcb76986fb2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_print_game_stats</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a605e2e1374b4e7090941389e6a581a24</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>game_is_board_in_starting_position</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a3534f1702346800f3c81568d62f497bc</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_initialize_board</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a9b4b4ef6ba4260c09a59192fc68be834</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_reset_game</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac75becc8b5888b8fa31045d7c28c085e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_start_new_game</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a57e990b9360540c1ba82ea3d5a32cf44</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_valid_position</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a93dee8ccd518ae0092501007a420e4bb</anchor>
      <arglist>(int row, int col)</arglist>
    </member>
    <member kind="function">
      <type>piece_t</type>
      <name>game_get_piece</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ab1f6294f23b24a27514d95c3b0ca0a4d</anchor>
      <arglist>(int row, int col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_set_piece</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a3fc7b98c71a5a479e4f1965cb8229074</anchor>
      <arglist>(int row, int col, piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_empty</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a2f77ab43a39a2a3ebc640df092f9cdc1</anchor>
      <arglist>(int row, int col)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_white_piece</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a8f69768eebdeba16538566a535f25351</anchor>
      <arglist>(piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_black_piece</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a304560a99cb8733e12bdcd9461b66926</anchor>
      <arglist>(piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_same_color</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>afd3cb102b201b005212505e9fcc86612</anchor>
      <arglist>(piece_t piece1, piece_t piece2)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_opponent_piece</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a7f6d6a865d66f62cfe46e3e29212f872</anchor>
      <arglist>(piece_t piece, player_t player)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_is_valid_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a20c746cc968a038cbdf28e01a9dd8307</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_valid_move_bool</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aefd2619f5dc787e72db00b8c24ecc405</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_piece_move_enhanced</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a2c589b4bfa6cd43ea3582050e37c1cc4</anchor>
      <arglist>(const chess_move_t *move, piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_validate_piece_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ae7cbb1bf49cc0ab8394c2015b2adbaa1</anchor>
      <arglist>(const chess_move_t *move, piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_pawn_move_enhanced</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a9fd338ce879b00b98ddb9d6b6988e5a1</anchor>
      <arglist>(const chess_move_t *move, piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_validate_pawn_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a9cd825eb0de3ca38648e5543dad28d45</anchor>
      <arglist>(const chess_move_t *move, piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_knight_move_enhanced</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a530c17b05f532aeaaa64d8bdb77ca665</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_validate_knight_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5d3594a8fc5be27b2309559f535ed67d</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_bishop_move_enhanced</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a94bb4afd94650885459c130f6c3a811d</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_validate_bishop_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a6d303839b2a0088558c5b56452232f4b</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_rook_move_enhanced</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a4b858d85c199815887890cf595969c75</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_validate_rook_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ad41adafacc7ea4384a6b7337ddea7299</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_queen_move_enhanced</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>acb3d6c97b1ab4160a2ed398cfa3b5b52</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_validate_queen_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a29083b28e0ddc03048382e5c8e73c200</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_king_move_enhanced</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a210042709363b26c91b23a04ccac022a</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_validate_king_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ab0a42f4f2685e64e790a1eb2a5df5530</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_would_move_leave_king_in_check</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>af6d73d7ab8a4c7b188d690a5177a0072</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_en_passant_possible</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a175e65acc9f386834730a86b25d6190a</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_castling</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a7169001872522d780e6e67a15d86a47e</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>print_error_detail</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a8025d8136c027e3884d5e8cbe8ee147a</anchor>
      <arglist>(const char *label, const char *fmt,...)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_display_move_error</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a8d2513928ede14f22023e22c775c3033</anchor>
      <arglist>(move_error_t error, const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>game_get_piece_name</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ae11065cb566adb8002df27fc8edc2b67</anchor>
      <arglist>(piece_t piece)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static int</type>
      <name>append_square</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ad6648c67da9dcf078bc2e9a5b3eed081</anchor>
      <arglist>(char *buffer, size_t size, int pos, const char *square)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_move_suggestions</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a6e5f0e6f61004c63b6da0114e626c68b</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_available_moves</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a7feffd0ac688b65213940c59e6414b5a</anchor>
      <arglist>(uint8_t row, uint8_t col, move_suggestion_t *suggestions, uint32_t max_suggestions)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_execute_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a4ec624bee837dfc3ef8429dd6edfc961</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>game_state_t</type>
      <name>game_get_state</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac8bfd19bdb7f1bbe6986c0f007713f73</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>player_t</type>
      <name>game_get_current_player</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a98c28bfe096e1b2f5dd819da477efbe1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_print_board</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ad7894f48d6e00cfe140b99026be38b71</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_print_move_history</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a6852701612778b8ad618fa420a0b7dca</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>game_send_response_to_uart</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a94c2b5c6bf9d2da1132e5b32256f83ca</anchor>
      <arglist>(const char *message, bool is_error, QueueHandle_t response_queue)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>game_send_board_to_uart</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ace3afd7e286a0eb92ff013f9e6232577</anchor>
      <arglist>(QueueHandle_t response_queue)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>game_process_pickup_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a0216d9328bf82ee185835194077f1e33</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_drop_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a8de7eb2cbccf01d3c7ee792caaf9784e</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_invalid_move_error_with_blink</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a907213fbd2400f88f96bd2a5bb4ff98d</anchor>
      <arglist>(uint8_t error_row, uint8_t error_col)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>game_trigger_victory_animation</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a1e01fb94ceb1dc74ff280d87a35c1076</anchor>
      <arglist>(player_t winner)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_reset_error_recovery_state</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aa3dc2b9ee227e2a5302b2c1d908ec6d7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>game_generate_advantage_graph</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a09408e7fee7146bd08512755a44f6bae</anchor>
      <arglist>(char *buffer, size_t buffer_size, uint32_t game_duration, uint32_t total_moves)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>convert_notation_to_coords</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a20b79ef4675f25cd3f39ddcdaf65243c</anchor>
      <arglist>(const char *notation, uint8_t *row, uint8_t *col)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>convert_coords_to_notation</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ae79fef0478151bf7d3256446807a2bf7</anchor>
      <arglist>(uint8_t row, uint8_t col, char *notation)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_evaluate_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a91c043d9d5fe2af5faf77047de79256e</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_save_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aba851251f06714196f8f83ce51067f33</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_load_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aa88c01d5f4c3f59c0e295897856ca9f5</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_castle_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a22142c91365332eb732cca4b572b9383</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_promote_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a195ee37f585a1c6b2a5a54afaa8aea21</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_component_off_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a0b85cb19c69ebbac367687a06e818021</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_component_on_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>adb937b707476c4a4280f82e3c78b8a4f</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_endgame_white_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a8351a9f9e065e3dfbfe3bdd50cb60d4a</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_endgame_black_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aae1e22b6f0b25caf0dfd763097e84b4c</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_list_games_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>afc75720af79d21406b4818da5d502fe0</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_delete_game_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5b253e24e11c0ad3222fb3d064f4d4cc</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_promotion_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ae1fac0ebfd2541173adbc7d7d5639605</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_handle_piece_lifted</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a33f0d04999a0cc77d79d2e529a227be1</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_handle_piece_placed</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a696a56f72aa0226d23952ccbdfd20cd7</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_handle_matrix_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a73c95d04856c50d53520e346abedf4da</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_chess_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a13909ec7cf87ce1999cd9647689e46aa</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_commands</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac4ac5416414272fb1942f0269ea572c7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_handle_invalid_move_smart</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ae998755862849e203b40a085fde47541</anchor>
      <arglist>(const chess_move_t *move, move_error_t error)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_highlight_invalid_target_area</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>acd21a8521c05d2746643f0a674e9ee5d</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_highlight_valid_moves_for_piece</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac0310521f54ddea6598d78bf6e009713</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_handle_invalid_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>accc9ce251093d9ee6c29aea32234ad53</anchor>
      <arglist>(move_error_t error, const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_move_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a1e6678c31e3832807ff10c23bbcd517b</anchor>
      <arglist>(const void *move_cmd_ptr)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_move_animation</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5fb8c34959fb71ce48802b7757701dbb</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col, piece_t piece, piece_t captured)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_player_change_animation</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>acecde79c7924b9de393d93d82e662595</anchor>
      <arglist>(player_t previous_player, player_t current_player)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_test_move_animation</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ad58e853637de174a1bc4f6ad9556bbaa</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_test_player_change_animation</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a0054718644880d12b2f8ccb352644a2b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_test_castle_animation</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a38bb9a0b5b3cd868021ba1f05be228d6</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_test_promote_animation</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a54c0942839659e43c97af3b1c7aca7e9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_test_endgame_animation</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac06cb22933b494c00b27ecb35160eeaa</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_check_game_conditions</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ad50b0c62b7700e4422eba52d68e2a938</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_king_in_check</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a737d8907ff7e0ea86e3c46e70f3c3614</anchor>
      <arglist>(player_t player)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_has_legal_moves</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a90e272f6928e9145d30671ce523ff35e</anchor>
      <arglist>(player_t player)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_enemy_piece</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a4a1fae8d66a8db2dd28a091a3b11279c</anchor>
      <arglist>(piece_t piece, player_t player)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_insufficient_material</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a93a1f0d6a82af5e13f78ebecc8a5ed8c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>game_state_t</type>
      <name>game_check_end_game_conditions</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a3cd556600affcd7945ded58e0a05c9c3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_toggle_timer</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a92c446eef9cfeb30bdcdbf87546c561b</anchor>
      <arglist>(bool enabled)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_save_game</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a6710818895e93c8a7b6852ab80ad59bb</anchor>
      <arglist>(const char *game_name)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_load_game</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a6d7435b0a21ea4d6d2791e2a7cb665b5</anchor>
      <arglist>(const char *game_name)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_export_pgn</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a0a4948efe62eece9f97fd6eef22db6e8</anchor>
      <arglist>(char *pgn_buffer, size_t buffer_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_coords_to_square</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a36549782eb3387a93e25c7d73325bd9f</anchor>
      <arglist>(uint8_t row, uint8_t col, char *square)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_print_status</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a138dd79d240e644870a7d53580720206</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_task_start</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ab26d022ec766e04addd46d9b6da9768a</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_matrix_events</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a502098cd25ad56400beb2339a1bfb84c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_valid_square</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ad58024344b5777da494b4a35f8250c32</anchor>
      <arglist>(int row, int col)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_own_piece</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a7f31b3ea42f40399386306ba3d88cb9c</anchor>
      <arglist>(piece_t piece, player_t player)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_square_attacked</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a719de74e100448e17548867befc43a1b</anchor>
      <arglist>(uint8_t row, uint8_t col, player_t by_player)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_simulate_move_check</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a3d771c47f735ab5e75fee68915b073fc</anchor>
      <arglist>(chess_move_extended_t *move, player_t player)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_generate_pawn_moves</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aba34fdb2616d1c154dadc9d122338c80</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, player_t player)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_generate_knight_moves</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a62e1a3f62089cca625cf6121749115b1</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, player_t player)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_generate_sliding_moves</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a64663ccff188c00244631bbeb85da3e1</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, player_t player, const int8_t directions[][2], int num_directions)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_generate_king_moves</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>adb84cfa34e63ffebf55a5fc8784eb0a9</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, player_t player)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_move_enhanced</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac096591c429faefd9993ddcc3d87bf11</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_detect_new_game_setup</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ad6aef74bc8447dabf92abcb081340931</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_highlight_movable_pieces</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a484415c76302e466ea7b01ecb2edee09</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_detect_and_handle_castling</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a1bab09959bb451eacb7eadf20da5aff1</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_castling_rook_guidance</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>afd365d57cb897126d7c7df4547c45d25</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_check_castling_completion</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a6974209fc82de9bbb764a014d9badec0</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>show_castling_completion_animation</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>acf17b5ac5ef3ff609422be26351370df</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_start_castle_animation</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5163d6404ac51406bdeffbe99262725c</anchor>
      <arglist>(const chess_move_extended_t *move)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>rook_animation_timer_callback</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>af26d73c4a9e269473827e4d4497b21b3</anchor>
      <arglist>(TimerHandle_t xTimer)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_start_repeating_rook_animation</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>afbd09cfaddffe4661015d92975561bd2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_stop_repeating_rook_animation</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a400c845a7ea99010ffc662d32fc20147</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_castle_animation_active</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a67dfb69eac8af38766e7d92cf3b4b5d0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_castling_expected</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aa407236aeb356b0929ab54581fe041bb</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_complete_castle_animation</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a98af1de0d72580a78b6f87a672710bc7</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_get_board_json</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a6199d3c51c2ba715c27b7be7428c61b7</anchor>
      <arglist>(char *buffer, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_move_count</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a2e528b3b66fe59544f28ca993c4a6143</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_get_status_json</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>abdf3ba446d87db43c26b6c51275a521e</anchor>
      <arglist>(char *buffer, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_get_history_json</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5d08ec12c99532e3819abb072385a921</anchor>
      <arglist>(char *buffer, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_get_captured_json</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aa442a40b76d56106f726f76bc210b84e</anchor>
      <arglist>(char *buffer, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_get_advantage_json</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a8aacf784d572f6de13c441c564d8037e</anchor>
      <arglist>(char *buffer, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_get_timer_json</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a4373558ca8d873f8a649ab8436720807</anchor>
      <arglist>(char *buffer, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_set_time_control</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a3784bb9de91bf60f6bff17c25c220c37</anchor>
      <arglist>(const time_control_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_start_timer_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a76220c211d4e21aa36ac90a00761f698</anchor>
      <arglist>(bool is_white_turn)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_end_timer_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a95937d38b6f9490371d95bd51ec8460c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_pause_timer</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a25364833eb80653b4d30414376f3fe35</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_resume_timer</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aa1855cbe07e70ad38d0fbd163e7e9746</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_reset_timer</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a0df29c9e5d4723c41477e530799976b0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_check_timer_timeout</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aac9b313d5314a9f9c948b466052c1e06</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_remaining_time</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a58b489e87078e33ccf75b35d892bb4fe</anchor>
      <arglist>(bool is_white_turn)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_get_timer_state</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>af09ae67f3a64e8d2865bac5eb0d7e3ca</anchor>
      <arglist>(chess_timer_t *timer_data)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_init_timer_system</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a917f1907b236e1476e75bb033e2a3165</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_process_timer_command</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a7c1b38b6a6342df233fdcafe702b5a0f</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_handle_time_expiration</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a3b6831d8d640dff74119ccdb4f9ff9d2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_update_timer_display</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aab2504466c50dfa9455e1414810bc073</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_available_time_controls</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a7d42316d2411ec24cc4ccbae29fa3cd9</anchor>
      <arglist>(time_control_config_t *controls, uint32_t max_count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_set_custom_time_control</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a66ce03930668ef5869101bef9e2a8474</anchor>
      <arglist>(uint32_t minutes, uint32_t increment_seconds)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_get_timer_stats</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>af61ca7975c281efd16bd95628b65bd5e</anchor>
      <arglist>(uint32_t *total_moves, uint32_t *avg_move_time)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_timer_active</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a2f01e5a8d282157e3cd8bd5a03b9c8b5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>time_control_type_t</type>
      <name>game_get_current_time_control_type</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a7fb22aa3cdacc593dccf7d95db908750</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_refresh_leds</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a8b7aa3018c0775246c6fe867f3ae6af1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int8_t</type>
      <name>knight_moves</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aa8992a10c771c05333aaea769d0cc506</anchor>
      <arglist>[8][2]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static game_state_t</type>
      <name>current_game_state</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a6b5f091d5551e8348e4b43edde98e03f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static player_t</type>
      <name>current_player</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a39560fbeaa9a251526561b4f2a86eac1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>move_count</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a154fb9029ef76390cf7a42e2672c397b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>auto_new_game_blocked_until_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aeeb78940e65fff2e71ea41c1a8787c1c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static castling_state_t</type>
      <name>castling_state</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a78f8e370f447d9c7ebea62679a5ad0c5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>castle_animation_active</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a7fab2e1c6f61a44564d0c595da75dc05</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static chess_move_extended_t</type>
      <name>pending_castle_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aec956796e1629a92f299b1725070b1c2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static TimerHandle_t</type>
      <name>rook_animation_timer</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ae342d6a960ec01e2dfef5b8876cccb9b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>rook_animation_active</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a0c4431921d8a29ef2f1347ffae4941cf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>rook_from_row</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a2e655fa25f4bbdd873eabad03e266985</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>rook_from_col</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aaf08ecb764a4aa8e2ce3d75d9bcf58f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>rook_to_row</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ad71601a1741c8dcb2c45992ebfc373b0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>rook_to_col</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a802ed5327f737b0304fbc1809d0aac8f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static piece_t</type>
      <name>board</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>acd8995588803c845a38aeae9849f7be8</anchor>
      <arglist>[8][8]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>piece_moved</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a32ed58d8bde5af2ffdbdb87e51758601</anchor>
      <arglist>[8][8]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>piece_lifted</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>af81491a006e7a193e8fd657043851221</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>lifted_piece_row</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a879607e096b78cf354a1c67cd43b010a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>lifted_piece_col</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac6b4fa04af83701dd9196051e02d67f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static piece_t</type>
      <name>lifted_piece</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a39ddf6298a44867b27f8f0672eee5c96</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>capture_in_progress</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>af4bc372309f4e974d802cf688aaff7c9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>capture_target_row</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>acf3c29e0755d9839817e37766abf0841</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>capture_target_col</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac8abc30efd1065ba3e0ad3378ddedafe</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static piece_t</type>
      <name>capture_removed_piece</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a92739d92c8ffef161535f16fee1bbf44</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>opponent_piece_moved</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a43cb8f21829cb4cc35a7335fdf1465c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static piece_t</type>
      <name>opponent_piece_type</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a229b05a8e7118cfca7f6bec35c02334b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>opponent_original_row</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ae11c7fc1300aa53228b21f38059bda32</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>opponent_original_col</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a83b693be32848ae61a88d70f0c1eb629</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>opponent_current_row</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5ad5daaa132c46754e7718518aac40fd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>opponent_current_col</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aeb8179882039bb317d2467c1954ecf27</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>auto_new_game_in_starting_position</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aae9a4d0a6d4c8c30d7b645eacd06716f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>auto_new_game_timer_start</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a742eef556956d6c9f4b0c3276501dabd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>auto_new_game_triggered</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a04d3ba5e73ae60bb8c57f895a51db47b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>white_king_moved</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a0620fd0930d19a0b135c237682869d90</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>white_rook_a_moved</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a3864eaf443fa01680872e54e26e69e49</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>white_rook_h_moved</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ad84cf76d9d65212e80f52ce534b3d7bf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>black_king_moved</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a8bc4bd88f8a4b4dca7d334ac907df78b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>black_rook_a_moved</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a230aeeeb272e2891526956413febc269</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>black_rook_h_moved</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a52f58ae06711b813b98133559c278850</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>en_passant_available</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a01bb301cbe8700c281dbf823fc412923</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>en_passant_target_row</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a088deaf2db03b864150aa4b907da0ce7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>en_passant_target_col</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5d18dd08e04b69848244304316e48083</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>en_passant_victim_row</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a34994c75ae9b319265448cbd9b9a2c0c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>en_passant_victim_col</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a595cee17e3316a52530de0e4ecef9222</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static chess_move_t</type>
      <name>move_history</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>af35759f7837c69c445a70f697226b584</anchor>
      <arglist>[200]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>history_index</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a2342129729fb9edaa9cf56931d99c00e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>task_running</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a35141cb45623cb3b7b4d96da35ed1831</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>game_active</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ab2f53c552730de70a774cd254de77ff1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>has_invalid_piece</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aaf8f442c7b06c825971fa6dc67e06e66</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>invalid_row</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a3bc97aa032f065584d497d64d124632e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>invalid_col</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a571899e94ee6d82dbb7aae2c9038279d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>original_valid_row</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ae084bf0ad7337f75ef248cbeffa96672</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>original_valid_col</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ad7a22cba87b0f065d0e829f5e2ce887f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>piece_t</type>
      <name>piece_type</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a60252451b47846f576a0ceb38633b917</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>waiting_for_move_correction</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a0d6232ac151a791249d00522a315fdb3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>error_count</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a2029e9c9788de523ece2dabbc7ffe2c7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static struct @366327160374164250155112142342252024354316345053</type>
      <name>error_recovery_state</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a597a9f3b32563fba96f3041533db53da</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static non_blocking_blink_state_t</type>
      <name>blink_state</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aa911d0eb12e9acc41eb02246254cdb26</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>pending</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a39f16d585c4531d2bf2562631a775d73</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>square_row</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a6ff5d035c17e8a98f013994186eca92b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>square_col</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a196aae5a38f9e4efbf2b30c0958d62cd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>player_t</type>
      <name>player</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac5a8201c5f6bbce303ec36500d67b6d8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static struct @051071154177324341014242340137364374107037353325</type>
      <name>promotion_state</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a434aeae5dd0d1550ab0cad1e0a6b5f0f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static SemaphoreHandle_t</type>
      <name>promotion_mutex</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a63a7da38fbec25e9b4ca6591dbc94966</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static king_resignation_state_t</type>
      <name>resignation_state</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ab6b24338b18abfa681b8017ce9db9d79</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>total_games</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aedb793cda816bdac01c6e216df7a2860</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>white_wins</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a86665d38f77c7b87cfd267e81708df58</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>black_wins</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a1033ed34d6f3dd511442ce41cc79ffae</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>draws</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ae9bb73183d10c72b880fb4cd10d3f585</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>endgame_report_requested</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ada50d1ec543170004c480c898a2718c6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static endgame_reason_t</type>
      <name>current_endgame_reason</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>adef591657b12c25bdfa613166b89f076</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static last_move_type_t</type>
      <name>last_move_type</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a4310fc073a4924b686ab768c91bb6766</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>game_start_time</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ad42240bf8319b29277fbf734cb427aa5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>last_move_time</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a88728ddbe481a763027b941da8297c87</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>white_time_total</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a3d4dd73ea0cc420ac9d98dac91b8e2cd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>black_time_total</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a18290b02f1d4991d7fd4fe790ca0fecb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>white_moves_count</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ad3290a04aa3aca9932f3567c262e5a68</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>black_moves_count</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a717ec25ef6ee64f8fe8a8749fe9b50f4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>white_captures</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a54642ff5d4cd1124c5d9c20879d0a17e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>black_captures</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a77c6e1ccb5248a069792d8a97fd74cab</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>white_checks</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>afdd74432e6827c9e6b4ede5e8a85568f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>black_checks</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a1794518da5b69585365914ea4de9df8c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>white_castles</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ac467517c9f77dd562bc355b03452a599</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>black_castles</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a1327325ac2c835cb293b62d8a14bc30e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static piece_t</type>
      <name>white_captured_pieces</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5659a83d5bfbbf6f1dde862e173b7f85</anchor>
      <arglist>[16]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static piece_t</type>
      <name>black_captured_pieces</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a8fa6f675bba152e024abc896c6fe5502</anchor>
      <arglist>[16]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>white_captured_count</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a0cfc5f6377b9053f8ba0acdcbb649b89</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>black_captured_count</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a2420950523896e65e6f8119cf831cbbc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>white_captured_index</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a2f596338bf66b8c99a411bda0d7e4f05</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>black_captured_index</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a586ff3f9834832bd857999ba5fe7a754</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static int8_t</type>
      <name>material_advantage_history</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a919011f371063105bc6e73a40625e0ed</anchor>
      <arglist>[200]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>advantage_history_count</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>afb3b2e18e26654ce6ceb6b8ff455ad69</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>moves_without_capture</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a371c1b2d4cadfc3079414c671ed85847</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>max_moves_without_capture</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ab85ef2a6cc22b437e180ef15dfc4f043</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>position_history</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aacd4de9dfd40491ca20038a933dfdda4</anchor>
      <arglist>[100]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>position_history_count</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aa2cf15d7d0808a9a22c4eb6462c9153d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>timer_enabled</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a9f577310be55e50d099f72849f597a25</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>game_saved</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a1f70b0c8dbc0c485ab51c2892c75b959</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static char</type>
      <name>saved_game_name</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a37493b1f0bf22f8cfffaa5d37f6d589d</anchor>
      <arglist>[32]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static game_state_t</type>
      <name>game_result</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5e3dc04c60e763a8338084ef2d20d34f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static game_result_type_t</type>
      <name>current_result_type</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a4276036f284fa3441e791279608a4ffa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>piece_values</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a0f99785951affb36a4998ea958983b86</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>last_move_from_row</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a7f77e5e231fd9535d20f64f96ee418ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>last_move_from_col</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a39aa6c3a7f8c8803266df3d726a125e3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>last_move_to_row</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5b89325f96b3f1db097a5f20d0360672</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>last_move_to_col</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aa9a6df76b92bfaac37043bfc352766c6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>has_last_move</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aae58883f4e5761e9d840b243daf855a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>tutorial_mode_active</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a53e9e83a9003c07651e45d0f0c7cc594</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>show_hints</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>afae0eab069ccc976d3057e2faaef476b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>piece_symbols</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ae9609d2c6acb49bcfdc4a72a4ee02d06</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>fifty_move_counter</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>aa37fa03c5c45c9a925b4b41ac3cdd4f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int8_t</type>
      <name>king_moves</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a7ba91d42df01587a1fa707b8a6e65c8c</anchor>
      <arglist>[8][2]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int8_t</type>
      <name>bishop_dirs</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a74a955b0f7a508aa10c622ae8d7563eb</anchor>
      <arglist>[4][2]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int8_t</type>
      <name>rook_dirs</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>ad0b01bbbe6f8ae3314cff69f772b691f</anchor>
      <arglist>[4][2]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static chess_move_extended_t</type>
      <name>legal_moves_buffer</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a743bcb69a1d8c2d43776b4320477738a</anchor>
      <arglist>[128]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>legal_moves_count</name>
      <anchorfile>game__task_8c.html</anchorfile>
      <anchor>a5b26569ffdd2946a02907fb9875307cf</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>game_colors.h</name>
    <path>components/game_task/include/</path>
    <filename>game__colors_8h.html</filename>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_RED</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>a7f026776e12ac1b5e7736c24b756085d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_GREEN</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>ad050fd71ced44460a0c723a1715c40e5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_YELLOW</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>af6e8b5c7b35e8bff4c581efd8019c798</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_BLUE</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>a50a197ede59d7694515c3dad4557cf00</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_MAGENTA</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>a8a9c0853e05b02c03acdfaab90f534c8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_CYAN</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>a8e74e295b54795a32a16ea744f6aed5e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_GRAY</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>a81a44be9e5e45edcb6be0d83133c1aa0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_WHITE</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>a141dae929732a29e6654ebc4741d6c93</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_BOLD</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>a46587b83544b8e9be045bb93f644ac82</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_DIM</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>a51fea81bd0f75dcc2fc1b341d9cc7958</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_RESET</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>af77a445894f2f750d43cf2182cd29e55</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>FMT_ERROR</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>a60d4e1d55cc7102123ffe5c3539ec165</anchor>
      <arglist>(msg)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>FMT_INFO</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>abd9912e75415821ab3192e7cf4542b6d</anchor>
      <arglist>(msg)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>FMT_HINT</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>a3bfaa4ef17d08f922b975ab28af9b499</anchor>
      <arglist>(msg)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>FMT_DATA</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>a4c5526bcd50bcf745bcbebcd4bc1b1e4</anchor>
      <arglist>(msg)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>disable_all_colors</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>ad8228dde623643def2b9c5f77bfe178f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>g_colors_enabled</name>
      <anchorfile>game__colors_8h.html</anchorfile>
      <anchor>a922aae80bb2a95e80fc13e4badd82b2a</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>game_led_direct.h</name>
    <path>components/game_task/include/</path>
    <filename>game__led__direct_8h.html</filename>
    <includes id="chess__types_8h" name="chess_types.h" local="yes" import="no" module="no" objc="no">chess_types.h</includes>
    <member kind="function">
      <type>void</type>
      <name>game_show_move_direct</name>
      <anchorfile>game__led__direct_8h.html</anchorfile>
      <anchor>a65d45d8068e75012a3bf91d813a86993</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_piece_lift_direct</name>
      <anchorfile>game__led__direct_8h.html</anchorfile>
      <anchor>aad4c6f489504e1aace2045420bcec3d1</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_valid_moves_direct</name>
      <anchorfile>game__led__direct_8h.html</anchorfile>
      <anchor>af337fa678e02039015341d7fa45e3ccc</anchor>
      <arglist>(uint8_t *valid_positions, uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_error_direct</name>
      <anchorfile>game__led__direct_8h.html</anchorfile>
      <anchor>a11c32da99485731ac1e0cae4f2174fcd</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_clear_highlights_direct</name>
      <anchorfile>game__led__direct_8h.html</anchorfile>
      <anchor>a8b43b89b88ce59ea25698ac5a8280bf2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_state_direct</name>
      <anchorfile>game__led__direct_8h.html</anchorfile>
      <anchor>a002b9dd58ec7cfbcac57367df453f258</anchor>
      <arglist>(piece_t *board)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_check_direct</name>
      <anchorfile>game__led__direct_8h.html</anchorfile>
      <anchor>a0ace822d82e2da6de18559787be957d0</anchor>
      <arglist>(uint8_t king_row, uint8_t king_col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_checkmate_direct</name>
      <anchorfile>game__led__direct_8h.html</anchorfile>
      <anchor>aea44edb838b9522e00dfd55da1856986</anchor>
      <arglist>(uint8_t king_row, uint8_t king_col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_promotion_direct</name>
      <anchorfile>game__led__direct_8h.html</anchorfile>
      <anchor>a47ce5497c44b6af58ed04cc1c3d806b8</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_castling_direct</name>
      <anchorfile>game__led__direct_8h.html</anchorfile>
      <anchor>aa112414da6cba72225e1b2958c958fa3</anchor>
      <arglist>(uint8_t king_from_row, uint8_t king_from_col, uint8_t king_to_row, uint8_t king_to_col, uint8_t rook_from_row, uint8_t rook_from_col, uint8_t rook_to_row, uint8_t rook_to_col)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>game_task.h</name>
    <path>components/game_task/include/</path>
    <filename>game__task_8h.html</filename>
    <includes id="timer__system_8h" name="timer_system.h" local="yes" import="no" module="no" objc="no">../../timer_system/include/timer_system.h</includes>
    <includes id="chess__types_8h" name="chess_types.h" local="yes" import="no" module="no" objc="no">chess_types.h</includes>
    <member kind="function">
      <type>bool</type>
      <name>convert_notation_to_coords</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a20b79ef4675f25cd3f39ddcdaf65243c</anchor>
      <arglist>(const char *notation, uint8_t *row, uint8_t *col)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>convert_coords_to_notation</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>ae79fef0478151bf7d3256446807a2bf7</anchor>
      <arglist>(uint8_t row, uint8_t col, char *notation)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_chess_move</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a13909ec7cf87ce1999cd9647689e46aa</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_get_board_json</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a6199d3c51c2ba715c27b7be7428c61b7</anchor>
      <arglist>(char *buffer, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_move_count</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a2e528b3b66fe59544f28ca993c4a6143</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_get_status_json</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>abdf3ba446d87db43c26b6c51275a521e</anchor>
      <arglist>(char *buffer, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_refresh_leds</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a8b7aa3018c0775246c6fe867f3ae6af1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_get_history_json</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a5d08ec12c99532e3819abb072385a921</anchor>
      <arglist>(char *buffer, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_get_captured_json</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>aa442a40b76d56106f726f76bc210b84e</anchor>
      <arglist>(char *buffer, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_get_advantage_json</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a8aacf784d572f6de13c441c564d8037e</anchor>
      <arglist>(char *buffer, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_task_start</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>ab26d022ec766e04addd46d9b6da9768a</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_initialize_board</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a9b4b4ef6ba4260c09a59192fc68be834</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_reset_game</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>ac75becc8b5888b8fa31045d7c28c085e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_start_new_game</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a57e990b9360540c1ba82ea3d5a32cf44</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_reset_error_recovery_state</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>aa3dc2b9ee227e2a5302b2c1d908ec6d7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_valid_position</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a93dee8ccd518ae0092501007a420e4bb</anchor>
      <arglist>(int row, int col)</arglist>
    </member>
    <member kind="function">
      <type>piece_t</type>
      <name>game_get_piece</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>ab1f6294f23b24a27514d95c3b0ca0a4d</anchor>
      <arglist>(int row, int col)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_empty</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a2f77ab43a39a2a3ebc640df092f9cdc1</anchor>
      <arglist>(int row, int col)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_white_piece</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a8f69768eebdeba16538566a535f25351</anchor>
      <arglist>(piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_black_piece</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a304560a99cb8733e12bdcd9461b66926</anchor>
      <arglist>(piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_same_color</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>afd3cb102b201b005212505e9fcc86612</anchor>
      <arglist>(piece_t piece1, piece_t piece2)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_is_valid_move</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a20c746cc968a038cbdf28e01a9dd8307</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_piece_move_enhanced</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a2c589b4bfa6cd43ea3582050e37c1cc4</anchor>
      <arglist>(const chess_move_t *move, piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_pawn_move_enhanced</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a9fd338ce879b00b98ddb9d6b6988e5a1</anchor>
      <arglist>(const chess_move_t *move, piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_knight_move_enhanced</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a530c17b05f532aeaaa64d8bdb77ca665</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_bishop_move_enhanced</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a94bb4afd94650885459c130f6c3a811d</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_rook_move_enhanced</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a4b858d85c199815887890cf595969c75</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_queen_move_enhanced</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>acb3d6c97b1ab4160a2ed398cfa3b5b52</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_king_move_enhanced</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a210042709363b26c91b23a04ccac022a</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_would_move_leave_king_in_check</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>af6d73d7ab8a4c7b188d690a5177a0072</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_en_passant_possible</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a175e65acc9f386834730a86b25d6190a</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>move_error_t</type>
      <name>game_validate_castling</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a7169001872522d780e6e67a15d86a47e</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_insufficient_material</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a93a1f0d6a82af5e13f78ebecc8a5ed8c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_move_suggestions</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a6e5f0e6f61004c63b6da0114e626c68b</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_available_moves</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a7feffd0ac688b65213940c59e6414b5a</anchor>
      <arglist>(uint8_t row, uint8_t col, move_suggestion_t *suggestions, uint32_t max_suggestions)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_execute_move</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a4ec624bee837dfc3ef8429dd6edfc961</anchor>
      <arglist>(const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>game_state_t</type>
      <name>game_get_state</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>ac8bfd19bdb7f1bbe6986c0f007713f73</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>player_t</type>
      <name>game_get_current_player</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a98c28bfe096e1b2f5dd819da477efbe1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_print_board</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>ad7894f48d6e00cfe140b99026be38b71</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>game_get_piece_name</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>ae11065cb566adb8002df27fc8edc2b67</anchor>
      <arglist>(piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_commands</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>ac4ac5416414272fb1942f0269ea572c7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_handle_invalid_move</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>accc9ce251093d9ee6c29aea32234ad53</anchor>
      <arglist>(move_error_t error, const chess_move_t *move)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_player_change_animation</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>acecde79c7924b9de393d93d82e662595</anchor>
      <arglist>(player_t previous_player, player_t current_player)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_test_move_animation</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>ad58e853637de174a1bc4f6ad9556bbaa</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_test_player_change_animation</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a0054718644880d12b2f8ccb352644a2b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_test_castle_animation</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a38bb9a0b5b3cd868021ba1f05be228d6</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_test_promote_animation</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a54c0942839659e43c97af3b1c7aca7e9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_test_endgame_animation</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>ac06cb22933b494c00b27ecb35160eeaa</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_move_direct</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a65d45d8068e75012a3bf91d813a86993</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_check_direct</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a0ace822d82e2da6de18559787be957d0</anchor>
      <arglist>(uint8_t king_row, uint8_t king_col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_player_change_direct</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>abc7c8755788427711faa337e69d53e19</anchor>
      <arglist>(player_t current_player)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_clear_highlights_direct</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a8b43b89b88ce59ea25698ac5a8280bf2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_piece_lift_direct</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>aad4c6f489504e1aace2045420bcec3d1</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_valid_moves_direct</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>af337fa678e02039015341d7fa45e3ccc</anchor>
      <arglist>(uint8_t *valid_positions, uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_invalid_move_error</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a5f2645fc55410cfe671fe6e8f2a99f2f</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_button_error</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a94723cd4e9d7f319e96c1d7aaa67fcf8</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_castling_guidance</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a528221cd8d3c2dd6de0732883230033a</anchor>
      <arglist>(uint8_t king_row, uint8_t king_col, uint8_t rook_row, uint8_t rook_col, bool is_kingside)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_check_game_conditions</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>ad50b0c62b7700e4422eba52d68e2a938</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_coords_to_square</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a36549782eb3387a93e25c7d73325bd9f</anchor>
      <arglist>(uint8_t row, uint8_t col, char *square)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_print_status</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a138dd79d240e644870a7d53580720206</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_print_game_stats</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a605e2e1374b4e7090941389e6a581a24</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_white_wins</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>ac06b42b52fc2abf90b66dcbd73ecd4a0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_black_wins</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a4c5423c283319ea94779b79682ca9b98</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_draws</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a49a6874590b30a62f04f7ca6d3efcf05</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_total_games</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>aed140b5d1ea696aa8fcd53aacc2303a9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>game_get_game_state_string</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a870a53ed0d150a3c293bcbcb76986fb2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_calculate_position_hash</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a2e80d005401f27a295b3f2b440d4bb88</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_position_repeated</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a009d7069912fb2689b7443575d3def38</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_add_position_to_history</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a7d059ee1bbc8427968feaf95af032112</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>game_calculate_material_balance</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a715422f9b34ce3aff72a852b7951ebba</anchor>
      <arglist>(int *white_material, int *black_material)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_get_material_string</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a13f42aca89279614ea9159dec44ae36c</anchor>
      <arglist>(char *buffer, size_t buffer_size)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_king_in_check</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a737d8907ff7e0ea86e3c46e70f3c3614</anchor>
      <arglist>(player_t player)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_has_legal_moves</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a90e272f6928e9145d30671ce523ff35e</anchor>
      <arglist>(player_t player)</arglist>
    </member>
    <member kind="function">
      <type>game_state_t</type>
      <name>game_check_end_game_conditions</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a3cd556600affcd7945ded58e0a05c9c3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_matrix_events</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a502098cd25ad56400beb2339a1bfb84c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_highlight_movable_pieces</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a484415c76302e466ea7b01ecb2edee09</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_detect_new_game_setup</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>ad6aef74bc8447dabf92abcb081340931</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_castle_animation_active</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a67dfb69eac8af38766e7d92cf3b4b5d0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_castling_expected</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>aa407236aeb356b0929ab54581fe041bb</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_complete_castle_animation</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a98af1de0d72580a78b6f87a672710bc7</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_start_repeating_rook_animation</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>afbd09cfaddffe4661015d92975561bd2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_stop_repeating_rook_animation</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a400c845a7ea99010ffc662d32fc20147</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_process_drop_command</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a8de7eb2cbccf01d3c7ee792caaf9784e</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>rook_animation_timer_callback</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>af26d73c4a9e269473827e4d4497b21b3</anchor>
      <arglist>(TimerHandle_t xTimer)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_highlight_invalid_target_area</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>acd21a8521c05d2746643f0a674e9ee5d</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_highlight_valid_moves_for_piece</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>ac0310521f54ddea6598d78bf6e009713</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_castling_rook_guidance</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>afd365d57cb897126d7c7df4547c45d25</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>show_castling_completion_animation</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>acf17b5ac5ef3ff609422be26351370df</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>game_show_invalid_move_error_with_blink</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a907213fbd2400f88f96bd2a5bb4ff98d</anchor>
      <arglist>(uint8_t error_row, uint8_t error_col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_get_timer_json</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a4373558ca8d873f8a649ab8436720807</anchor>
      <arglist>(char *buffer, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_start_timer_move</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a76220c211d4e21aa36ac90a00761f698</anchor>
      <arglist>(bool is_white_turn)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_end_timer_move</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a95937d38b6f9490371d95bd51ec8460c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_pause_timer</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a25364833eb80653b4d30414376f3fe35</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_resume_timer</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>aa1855cbe07e70ad38d0fbd163e7e9746</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_reset_timer</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a0df29c9e5d4723c41477e530799976b0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>game_get_remaining_time</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a58b489e87078e33ccf75b35d892bb4fe</anchor>
      <arglist>(bool is_white_turn)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_init_timer_system</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a917f1907b236e1476e75bb033e2a3165</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_process_timer_command</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a7c1b38b6a6342df233fdcafe702b5a0f</anchor>
      <arglist>(const chess_move_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_handle_time_expiration</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a3b6831d8d640dff74119ccdb4f9ff9d2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>game_update_timer_display</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>aab2504466c50dfa9455e1414810bc073</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_timer_active</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a2f01e5a8d282157e3cd8bd5a03b9c8b5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>game_is_promotion_available</name>
      <anchorfile>game__task_8h.html</anchorfile>
      <anchor>a303099e83402e02cace5235656259b70</anchor>
      <arglist>(player_t player)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>ha_light_task.c</name>
    <path>components/ha_light_task/</path>
    <filename>ha__light__task_8c.html</filename>
    <includes id="ha__light__task_8h" name="ha_light_task.h" local="yes" import="no" module="no" objc="no">ha_light_task.h</includes>
    <includes id="chess__types_8h" name="chess_types.h" local="yes" import="no" module="no" objc="no">chess_types.h</includes>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <includes id="game__task_8h" name="game_task.h" local="yes" import="no" module="no" objc="no">game_task.h</includes>
    <includes id="led__task_8h" name="led_task.h" local="yes" import="no" module="no" objc="no">led_task.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>MQTT_NVS_NAMESPACE</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a296a7bbb8593405108b39d1ab6c0ecc6</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MQTT_NVS_KEY_HOST</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a081583a2a1790d8fb1b2617f75e28001</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MQTT_NVS_KEY_PORT</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a86093592f3d60d17d74e7fb27193ea3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MQTT_NVS_KEY_USERNAME</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>aeeff4bcf4d1d0b27a9da374befe7bc8d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MQTT_NVS_KEY_PASSWORD</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>ab809ff3bf1caf2065817bd285c78889d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MQTT_DEFAULT_HOST</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a52bd6fcbbfadb261cdd62abc295f4dd5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MQTT_DEFAULT_PORT</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a47dbba2fb767e8362864895e3ce87a36</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MQTT_DEFAULT_USERNAME</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a67a711e15478d8d8ba7aa984947f1cd8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MQTT_DEFAULT_PASSWORD</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a38f208acaf4f9f771cf7e707488f8c29</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>ha_light_check_activity_timeout</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a5a6bb92dacbf05bc953f9d712c939ed4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>ha_light_switch_to_ha_mode</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>aff8e250ad01ba14909377eaea2f0dce6</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>ha_light_switch_to_game_mode</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a33c86dd0c42ba70448023639e56096f0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>ha_light_monitor_game_activity</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a1819a1e1ebc04fa6179f55ee77454506</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>ha_light_check_wifi_sta_connected</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a687dd5b61866fa4896abac67db3944d9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>ha_light_init_mqtt</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>ab445707e64ef5877d69ff4443c54bfd5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>ha_light_mqtt_event_handler</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>aaa50395cd97ca2238e5b5a64d0583d07</anchor>
      <arglist>(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>ha_light_handle_mqtt_command</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>ae13c9a71f7715bc11cf0c8dc511d7b33</anchor>
      <arglist>(const char *topic, const char *data, int data_len)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>ha_light_publish_state</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>ab17c9e100295b75e1b76d897d08a805b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>ha_light_task_wdt_reset_safe</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a220eb7a6a15e1c82a39c3128aeffa19f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ha_light_report_activity</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>af2c48fb0be8e0af2ec8f26872bea2721</anchor>
      <arglist>(const char *activity_type)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>mqtt_load_config_from_nvs</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>ad61c60d5f24e8689c8607c69c989b761</anchor>
      <arglist>(char *host, size_t host_len, uint16_t *port, char *username, size_t username_len, char *password, size_t password_len)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>mqtt_save_config_to_nvs</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>ad116a346513e475c7ffe3e5137a18d0d</anchor>
      <arglist>(const char *host, uint16_t port, const char *username, const char *password)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>ha_light_publish_discovery</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a0f43fcd5e5a6b5aa88a40bfbf987d381</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>mqtt_ensure_config_loaded</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>af54ee77cebff3b3c12301819b1082ce4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>ha_mode_t</type>
      <name>ha_light_get_mode</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a745b4de0bfe0480fe88c73e3d23170eb</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ha_light_is_available</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a0fc50519ca67911865aa6f8afc0a939f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>mqtt_get_config</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a0564ec8bd0c8c4e490ed84e22246df81</anchor>
      <arglist>(char *host, size_t host_len, uint16_t *port, char *username, size_t username_len, char *password, size_t password_len)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ha_light_is_mqtt_connected</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a7f38fb48c581f40707dbed343cedb07c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>ha_light_reinit_mqtt</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>aaa92358f60e1043bee0865be41fb1b2c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ha_light_task_start</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>adeb86d27a08d24abfd9a912c9c89a729</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>task_running</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a35141cb45623cb3b7b4d96da35ed1831</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static ha_mode_t</type>
      <name>current_mode</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a6f970625bbaaf55b63054a651a0329fd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>last_activity_time_ms</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a841339184f4dba27c769216e9b7e8a6f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>sta_connected</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a738f5dc6c12f15f0149e08de59f48064</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static esp_mqtt_client_handle_t</type>
      <name>mqtt_client</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>ad9332f302ae79b5195a71d791a9ccecd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>mqtt_connected</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a6a99fbef218ac40fc0b77e18f0e9a708</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>state</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>ab30ba07e2a0bd07a15e45a92c32db9c5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>brightness</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a837c871f200f8c3d19d0c7f2e031ffc4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>r</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a4c5c6ceb8ed33456261fa907136e0c3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>g</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a1673907d4d89d763bb7b94ec1eeb7b60</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>b</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a4313c9563516f94387762ab05763456b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>effect</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>af7b0cb88246dfc40187f86c43e8d18f0</anchor>
      <arglist>[32]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static struct @210247337070230032013260354220374100003104353375</type>
      <name>ha_light_state</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a6f7c9a2d773fd7d2e70af21f86a3a795</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>host</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a471a823d17bf7081fd4ebf84741f6758</anchor>
      <arglist>[128]</arglist>
    </member>
    <member kind="variable">
      <type>uint16_t</type>
      <name>port</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a8e0798404bf2cf5dabb84c5ba9a4f236</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>username</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>acfea5fbe8295640a1489399ceeb6c018</anchor>
      <arglist>[64]</arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>password</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>ae3a23efe266762659e03567d6afc26dd</anchor>
      <arglist>[64]</arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>loaded</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a992c70a16169035bc9e2e8f9953d91ed</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static struct @211342177250135123207242057200371264063325110270</type>
      <name>mqtt_config</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>acceb8828f50bb72cef2187d48db6b461</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>game_command_queue</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a4ed770cbe46604586e494bae84b62304</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>matrix_event_queue</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>a1b93ff1badbca8962927cb6fd837463e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static QueueHandle_t</type>
      <name>ha_light_cmd_queue</name>
      <anchorfile>ha__light__task_8c.html</anchorfile>
      <anchor>afa3c4fd245d700def50f8fbe578ca1c4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>ha_light_task.h</name>
    <path>components/ha_light_task/include/</path>
    <filename>ha__light__task_8h.html</filename>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <class kind="struct">ha_light_command_t</class>
    <member kind="define">
      <type>#define</type>
      <name>HA_ACTIVITY_TIMEOUT_AUTO_MS</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a59a4f99e5382fed36fc539ce0b1b79c5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_ACTIVITY_TIMEOUT_COMMAND_MS</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a59e49211d6a9fc9ca8c1481be531753d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_TOPIC_LIGHT_COMMAND</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a8252c914f758fbce0485c28d94e84b0c</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_TOPIC_LIGHT_STATE</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>ac8d834caaa63c1dfab5bfbdaf442f78f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_TOPIC_LIGHT_AVAILABILITY</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>ac2d5de1749d4171f82324a7276f6fb14</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_TOPIC_GAME_ACTIVITY</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>ab8a6d62c662a157b70d0fabc6c03e6fc</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_MQTT_PAYLOAD_ONLINE</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a6e17c7afd5219d2aad45fa7d87822f3b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_MQTT_PAYLOAD_OFFLINE</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a6842398162d1d365ca5d5c711ecb6145</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_DISCOVERY_PREFIX</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>accaea9ff6b9f1de5ca1c5625eab21cf4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_COMPONENT_LIGHT</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a88f6ce72b1c205860c6f824cf2b23d8e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_DEVICE_MANUFACTURER</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a2d3c04b509cf1d7315195a4cbcdd20a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_DEVICE_MODEL</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a166cbf522b7a67a3c366f928e06d66c3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_DEVICE_SW_VERSION</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a5303289f6afee06f476293223b01b8bb</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_MQTT_CLIENT_ID</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a3e86bf68640eb01f845f67eb32fbc9d6</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HA_MQTT_BROKER_PORT</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a5b297c2591120ddece81447ffd44844f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>ha_mode_t</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>af2a3f8e1a020167cca1ba6c844ba3588</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>HA_MODE_GAME</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>af2a3f8e1a020167cca1ba6c844ba3588a51024b9ef0b703bb96a18c6971dd1fcb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>HA_MODE_HA</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>af2a3f8e1a020167cca1ba6c844ba3588ab05fd74afbd0d17e1b0de540a49c60c5</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ha_light_task_start</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>adeb86d27a08d24abfd9a912c9c89a729</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>ha_mode_t</type>
      <name>ha_light_get_mode</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a745b4de0bfe0480fe88c73e3d23170eb</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ha_light_is_available</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a0fc50519ca67911865aa6f8afc0a939f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ha_light_report_activity</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>af2c48fb0be8e0af2ec8f26872bea2721</anchor>
      <arglist>(const char *activity_type)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>mqtt_save_config_to_nvs</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>ad116a346513e475c7ffe3e5137a18d0d</anchor>
      <arglist>(const char *host, uint16_t port, const char *username, const char *password)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>mqtt_get_config</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a0564ec8bd0c8c4e490ed84e22246df81</anchor>
      <arglist>(char *host, size_t host_len, uint16_t *port, char *username, size_t username_len, char *password, size_t password_len)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ha_light_is_mqtt_connected</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>a7f38fb48c581f40707dbed343cedb07c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>ha_light_reinit_mqtt</name>
      <anchorfile>ha__light__task_8h.html</anchorfile>
      <anchor>aaa92358f60e1043bee0865be41fb1b2c</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>led_state_manager.h</name>
    <path>components/led_state_manager/include/</path>
    <filename>led__state__manager_8h.html</filename>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <class kind="struct">led_manager_config_t</class>
    <class kind="struct">led_pixel_t</class>
    <class kind="struct">led_layer_state_t</class>
    <class kind="struct">led_state_t</class>
    <member kind="enumeration">
      <type></type>
      <name>led_layer_t</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>af4c342c31ff859428428924ca8a0231b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_LAYER_BACKGROUND</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>af4c342c31ff859428428924ca8a0231ba61ab18d844400801ca7fe9c2c267cfcb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_LAYER_PIECES</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>af4c342c31ff859428428924ca8a0231ba61181a2d6018dd35a6107f7c092723e2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_LAYER_MOVES</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>af4c342c31ff859428428924ca8a0231ba258749d3fd51d257da5e667a6055f583</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_LAYER_SELECTION</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>af4c342c31ff859428428924ca8a0231ba8924cdd0ec60caffea8ad4a03e3a1ef7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_LAYER_ANIMATION</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>af4c342c31ff859428428924ca8a0231baf40b6e6b5b1261fb2c325fd71a9b20ad</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_LAYER_STATUS</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>af4c342c31ff859428428924ca8a0231ba231ba4fa658ed751cbe6b230aa550325</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_LAYER_ERROR</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>af4c342c31ff859428428924ca8a0231ba48fc31f3c8eeefb7eee5050df35b02e9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_LAYER_GUI</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>af4c342c31ff859428428924ca8a0231bab7445688615f8f0b8afcb309d9b9c1c3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LED_LAYER_COUNT</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>af4c342c31ff859428428924ca8a0231ba0d615861daf40238f3c436677a7a581b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>blend_mode_t</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a48277ad0ecc8bc035e62eeb7541a1936</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BLEND_REPLACE</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a48277ad0ecc8bc035e62eeb7541a1936a922505d6fbd0555ead23c68328860786</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BLEND_ALPHA</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a48277ad0ecc8bc035e62eeb7541a1936ac22cd29896978661961661ae66e10aaf</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BLEND_ADDITIVE</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a48277ad0ecc8bc035e62eeb7541a1936a474757d49ba1ecef3d2eccfd634cc2c9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BLEND_MULTIPLY</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a48277ad0ecc8bc035e62eeb7541a1936af638d6eb6f6ba6ba6bf7c0a91c21d006</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BLEND_OVERLAY</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a48277ad0ecc8bc035e62eeb7541a1936aa0025b6b86ea4dd13efeddac7a2a0997</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BLEND_MODE_COUNT</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a48277ad0ecc8bc035e62eeb7541a1936aa34c61bac9de4434917e72883a3f6bfb</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_init</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>ad9f316f87a8f2534ae9a3ee108be68fe</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_manager_init</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>ad6abb4aadf5a51d12bd7666f7199c724</anchor>
      <arglist>(const led_manager_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_deinit</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>af5f999fe890f5724b25095beb978b862</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_manager_deinit</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a0e1288bada603c6fa05e27c504043639</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_config</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a34e79246641dfad5e2ea7e2fa2129163</anchor>
      <arglist>(const led_manager_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_update_frequency</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>ab2f7c69ab57eff54259e52d5bdb36fc0</anchor>
      <arglist>(uint8_t frequency_hz)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_transition_duration</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a7f2c282caa434a66a0a90e65374603a0</anchor>
      <arglist>(uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_hsv_to_rgb</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a1123bf9a4fa107a05fc3251ae1c7d009</anchor>
      <arglist>(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_set_pixel</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>abcecdc9fd4b3c11699c419833d88b78c</anchor>
      <arglist>(led_layer_t layer, uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_pixel_layer</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>abe61cff6332fff6f402834c6aaf32ca7</anchor>
      <arglist>(led_layer_t layer, uint8_t led_index, uint8_t r, uint8_t g, uint8_t b)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_get_pixel</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a2307573e8d47caa7b85ee64ec6648b29</anchor>
      <arglist>(led_layer_t layer, uint8_t row, uint8_t col, led_pixel_t *pixel)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_clear_layer</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>af91344b4c14d67babc82db59768efe4c</anchor>
      <arglist>(led_layer_t layer)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_clear_layer</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a710d786eed5ca03bce7a093daf527041</anchor>
      <arglist>(led_layer_t layer)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_clear_all_layers</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>acbe73463d88146c9eb0e3bb11aacf5eb</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_set_layer_enabled</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a1858cf9b6879c4689512673dd80e8569</anchor>
      <arglist>(led_layer_t layer, bool enabled)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_set_blend_mode</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a06071787052558dd465741e015110788</anchor>
      <arglist>(led_layer_t layer, blend_mode_t mode)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_set_layer_alpha</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>aeb9b49e54933b8c8ef697035e767a946</anchor>
      <arglist>(led_layer_t layer, uint8_t alpha)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_compose</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>af09d4ac3694303c8022504b49184269f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_state_blend_pixel</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>afe499a84d57f8fbb92c12027f20d9b68</anchor>
      <arglist>(const led_pixel_t *bottom, const led_pixel_t *top, blend_mode_t mode, led_pixel_t *result)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_update_dirty</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a6b1925bc1d8c75c69ce4cbaf134389df</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_update_all</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a69062c8aaadfa4671d2cd933eede7e6b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_force_full_update</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a3f91b15c961fecfee68b7367424b4a39</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_state_mark_dirty</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a9aa3e95a3cb8b86c88d08728ac999b14</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_state_mark_all_dirty</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>abd8b268dd63b496803ea4251cc68d95a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_state_clear_dirty_flags</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a6a4d0daaae2ff091dbdca1b728b3bed5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_set_layer_bulk</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a81024d097096788d526bae15a7f90002</anchor>
      <arglist>(led_layer_t layer, const led_pixel_t *pixels)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_fade_layer</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>ad6aaf69c42e160ec0a7f6ae2ce4dc7d8</anchor>
      <arglist>(led_layer_t layer, uint8_t target_alpha, uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_apply_gamma</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a05a4f73280afa625afe196d1929a08a3</anchor>
      <arglist>(led_layer_t layer, float gamma)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_state_apply_brightness</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>ae84d0eef4aee0725d2091bf704e3d6f8</anchor>
      <arglist>(led_layer_t layer, uint8_t brightness)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_state_print_status</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>ad5642ff9fd0cb39d891b4170094b32d0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_state_print_layer</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>ad384df52caf815b47020c26a0eb98045</anchor>
      <arglist>(led_layer_t layer)</arglist>
    </member>
    <member kind="function">
      <type>const led_state_t *</type>
      <name>led_state_get_global</name>
      <anchorfile>led__state__manager_8h.html</anchorfile>
      <anchor>a1127332859ff92abb0faafcaad155233</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>led_state_manager.c</name>
    <path>components/led_state_manager/</path>
    <filename>led__state__manager_8c.html</filename>
    <includes id="led__state__manager_8h" name="led_state_manager.h" local="yes" import="no" module="no" objc="no">led_state_manager.h</includes>
    <includes id="led__task_8h" name="led_task.h" local="yes" import="no" module="no" objc="no">led_task.h</includes>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>led_composite_pixel</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>ab0e2cf384e338253fa57595534da19e5</anchor>
      <arglist>(uint8_t led_index)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>led_apply_brightness</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a0f4e2e8c4d259c4be234a5369001db20</anchor>
      <arglist>(uint8_t led_index, uint8_t *r, uint8_t *g, uint8_t *b)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>led_mark_dirty</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>af99a3c5e617122fc1975cb7516f67cb7</anchor>
      <arglist>(uint8_t led_index)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>led_clear_dirty</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a6f1add3d4b49d1580292fa88b8a33d29</anchor>
      <arglist>(uint8_t led_index)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>led_is_layer_enabled</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>aa543c3e91a16b3fdde64309d7c1742a7</anchor>
      <arglist>(led_layer_t layer)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static uint8_t</type>
      <name>led_get_layer_brightness</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>aede0d4ee4414771f62812fb528746af4</anchor>
      <arglist>(led_layer_t layer)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_manager_init</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>ad6abb4aadf5a51d12bd7666f7199c724</anchor>
      <arglist>(const led_manager_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_manager_deinit</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a0e1288bada603c6fa05e27c504043639</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_pixel_layer</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>abe61cff6332fff6f402834c6aaf32ca7</anchor>
      <arglist>(led_layer_t layer, uint8_t led_index, uint8_t r, uint8_t g, uint8_t b)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_clear_layer</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a710d786eed5ca03bce7a093daf527041</anchor>
      <arglist>(led_layer_t layer)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_layer_opacity</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a80147ffbd0f8c6fe9e9390dd710195e3</anchor>
      <arglist>(led_layer_t layer, uint8_t opacity)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_enable_layer</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a393b301ba78a74e2768530ee7b86573f</anchor>
      <arglist>(led_layer_t layer, bool enable)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_layer_brightness</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a40289bfb1bef83bce9cd41e4a5306545</anchor>
      <arglist>(led_layer_t layer, uint8_t brightness)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_composite_and_update</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>aeff6d8517fb4956b1d4df351544c57bb</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_force_full_update</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a3f91b15c961fecfee68b7367424b4a39</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>led_get_dirty_count</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a571aad292e2263e9d782457f8e026e0f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>led_get_dirty_count_by_layer</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a23eaaf20c7e72afecbdcb21bb9b2af23</anchor>
      <arglist>(led_layer_t layer)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_fade_pixel</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>ac2f041e67d008f43c140ba461a346e0a</anchor>
      <arglist>(uint8_t led_index, uint8_t target_r, uint8_t target_g, uint8_t target_b, uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_pulse_pixel</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>aba0f2c2275e342c31ec8ac161299b0ef</anchor>
      <arglist>(uint8_t led_index, uint8_t r, uint8_t g, uint8_t b, uint32_t period_ms, uint8_t pulse_count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_rainbow_pixel</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>ad5f296e945ac223050cba2e4264701ba</anchor>
      <arglist>(uint8_t led_index, uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_multiple_pixels</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a9814a093e435d1f453c7ecbd64817d5f</anchor>
      <arglist>(led_layer_t layer, uint8_t *led_indices, uint8_t count, uint8_t r, uint8_t g, uint8_t b)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_clear_multiple_pixels</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>ad188631e97fc317e2376f2b4c65ef62e</anchor>
      <arglist>(led_layer_t layer, uint8_t *led_indices, uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_fade_multiple_pixels</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>af8241d26504ce9a50bd8f1d05d29aaa9</anchor>
      <arglist>(uint8_t *led_indices, uint8_t count, uint8_t target_r, uint8_t target_g, uint8_t target_b, uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_global_brightness</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a79a1e941945444641da26f81f396a0ea</anchor>
      <arglist>(uint8_t brightness)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_pixel_brightness</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a09ad253b4572afc82ba7b977fb8be163</anchor>
      <arglist>(uint8_t led_index, uint8_t brightness)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>led_get_global_brightness</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>af3727770a26c0a0e796cab4969fcd909</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>led_get_pixel_brightness</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a560bcb09877849b8df1bce3f370dfa50</anchor>
      <arglist>(uint8_t led_index)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_hsv_to_rgb</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a1123bf9a4fa107a05fc3251ae1c7d009</anchor>
      <arglist>(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_rgb_to_hsv</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a6bf9d2280f7e13b64c909142e7158fd4</anchor>
      <arglist>(uint8_t r, uint8_t g, uint8_t b, float *h, float *s, float *v)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_interpolate_color</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>ae6932cf91967f91c556ac275b48ec9fb</anchor>
      <arglist>(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2, float progress, uint8_t *out_r, uint8_t *out_g, uint8_t *out_b)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_get_status</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a4768576d3c7535b5f97b6a0464b7e9c2</anchor>
      <arglist>(char *buffer, size_t buffer_size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_get_layer_status</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>ad10f07d8155009ce798f6b9bb7f84bda</anchor>
      <arglist>(led_layer_t layer, char *buffer, size_t buffer_size)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>led_is_pixel_dirty</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a229cf0585f4420c02ff501bf89c88471</anchor>
      <arglist>(uint8_t led_index)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>led_get_last_update_time</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>af25cb1c55819d4184e7270d9ab9ee0a1</anchor>
      <arglist>(uint8_t led_index)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_config</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a34e79246641dfad5e2ea7e2fa2129163</anchor>
      <arglist>(const led_manager_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_update_frequency</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>ab2f7c69ab57eff54259e52d5bdb36fc0</anchor>
      <arglist>(uint8_t frequency_hz)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_transition_duration</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a7f2c282caa434a66a0a90e65374603a0</anchor>
      <arglist>(uint32_t duration_ms)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>manager_initialized</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a056b36df10f165cd5a6b993fc63fa152</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static led_manager_config_t</type>
      <name>current_config</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a68b7b8603078ea293af759db43776975</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static led_layer_state_t</type>
      <name>layers</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a0ce524f4e46479bfbd62b2da84ed3922</anchor>
      <arglist>[LED_LAYER_COUNT]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>global_brightness</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>ac96aaee8c56d42372878aa20a24362c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>last_update_time</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a0bb6dcf519e5bcc59b766b1a23c2535c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>dirty_count</name>
      <anchorfile>led__state__manager_8c.html</anchorfile>
      <anchor>a660e94e6e13b2e261064fed4b752f20b</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>led_task.h</name>
    <path>components/led_task/include/</path>
    <filename>led__task_8h.html</filename>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <member kind="function">
      <type>void</type>
      <name>led_task_start</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a5c2b466a04fcadba97ca90ee811927d8</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_clear_all</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a323c87898912a4a4d62383dbe8bd7099</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_process_commands</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a07ea6d48b1deb7fd8ecb18cf6d7753be</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_update_hardware</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>ae35498b645fad1892ac45a425bf78cce</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_brightness_global</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>aba7d182926787e311733fb41353c196e</anchor>
      <arglist>(uint8_t brightness)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_execute_command_new</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a85578cf6b1c0acb68c6278c9cd1a4f26</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_pixel_internal</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a7cb7bb0774dead39c3607d19de9f9d34</anchor>
      <arglist>(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_all_internal</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a6d5be6d6368f979daf99dea8b1685f55</anchor>
      <arglist>(uint8_t red, uint8_t green, uint8_t blue)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_clear_all_internal</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a50360691268da244abb2711dc712bd54</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_show_chess_board</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a13da231e8e3dc956e4c3415a89409670</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_button_feedback</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>aa74e5b3b49114f8e7f69f346beaf5cd3</anchor>
      <arglist>(uint8_t button_id, bool available)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_button_press</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a8a31606dca399a00351e3ef16cc3a737</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_button_release</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a8c05ca5afd0c6a5184c88ee8e05f37b0</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>led_get_button_color</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a949ef87b771a33a9998649a33db6848d</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_button_promotion_available</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>ac30980ab24cfbd6fe5684d6ade1c1f7a</anchor>
      <arglist>(uint8_t button_id, bool available)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_start_animation</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a571ae664c39755862a8b153447841fe3</anchor>
      <arglist>(uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_test_pattern</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a909c49ae2d15c636755d044ad2380a74</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_print_compact_status</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a4f270f2071fdb90dc6da3d8d6d0e7fe2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_print_detailed_status</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a57338ebc4262ba916e9a68acdc8634fa</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_print_changes_only</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a39e8c14550a437ae7b94b56d8583b27a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_start_quiet_period</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>ad7200d35f2087aa4d23aa18fb79e87a5</anchor>
      <arglist>(uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_pixel_safe</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>ad4b7ef962f86de71ef5f934f2a92544a</anchor>
      <arglist>(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_clear_all_safe</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>afff280b4c9b7261d1a086e0cfa356604</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_all_safe</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a67410e7a666d076402f2eaf116716a2b</anchor>
      <arglist>(uint8_t red, uint8_t green, uint8_t blue)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_force_immediate_update</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a35ed753e036a2372ea57df822d92f154</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_clear_board_only</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>ab245ceabb580934b6117f59343c6d9a2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_clear_buttons_only</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a4f7ff7068535f3818fb37654f5bffdad</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_preserve_buttons</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a4653a6179cbaf07289307ce6c1871858</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_update_button_availability_from_game</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a31b820a4bfe33d0cf54a107c339d6d07</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_ha_color</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>acd139d9f40454ed76aa1f080f2299d07</anchor>
      <arglist>(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_restore_chess_board</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>aed864c4fd31d53e8498014ffda17dcc6</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_refresh_all_button_leds</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>acffe67fadfe40ddc4c14618edebeedf8</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_error_invalid_move</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a1a82f432b46c4990df10c3c8c4d697b4</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_error_return_piece</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a5fed0119b12c12bf3cee449144e40d43</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_anim_endgame</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a69379eed351dd045d9d23cda7ec86a69</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_error_recovery</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a8811aae10a60d7851edfc7af2ef69771</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_show_legal_moves</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a473fe800a9a37ba1caee73edd0398505</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_update_game_state</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>af7a7e8b554370642f118c98fd1015e86</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_highlight_pieces_that_can_move</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a320f9c61a3b7d32d1244558bc94affd4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_highlight_possible_moves</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a54ef8b8e4b3a77b63a1968de1f488759</anchor>
      <arglist>(uint8_t from_square)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_clear_all_highlights</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a3f8208e54db356a3a750b566d5bd763f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_player_change_animation</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a7e000dafeeac5016d70d5d182b89c10f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>led_has_significant_changes</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a13a1293f8ac28cbf7cd0d7b08cb58c5c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>led_get_led_state</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a444a170336ec287da201e2899805812b</anchor>
      <arglist>(uint8_t led_index)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_get_all_states</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>ab7562384ca3de074cd4f13eb8e96c790</anchor>
      <arglist>(uint32_t *states, size_t max_count)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_setup_animation_after_endgame</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a38353130f2210565c3c71c96f05ecf04</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_stop_endgame_animation</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a081382ca7f7a773156291851fdbe6f5f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_booting_animation</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a533dfd80b93f0821af12a5f929dcc40b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>led_is_booting</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a006a24ec8e7f302c4a6f8481675803d1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_boot_animation_step</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>ad9333c97e3a0d22851f18f91e890e3dc</anchor>
      <arglist>(uint8_t progress_percent)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_boot_animation_fade_out</name>
      <anchorfile>led__task_8h.html</anchorfile>
      <anchor>a51cc23ac460421224cfaf847669ab1d9</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>led_task_simple.h</name>
    <path>components/led_task/include/</path>
    <filename>led__task__simple_8h.html</filename>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_pixel_safe</name>
      <anchorfile>led__task__simple_8h.html</anchorfile>
      <anchor>ac98160bcc73acb81149019839363cd74</anchor>
      <arglist>(uint8_t index, uint8_t r, uint8_t g, uint8_t b)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_clear_all_safe</name>
      <anchorfile>led__task__simple_8h.html</anchorfile>
      <anchor>a89dbb2b29ddb1afaeadee1dd99cab308</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_all_safe</name>
      <anchorfile>led__task__simple_8h.html</anchorfile>
      <anchor>a21ac075b956f179c2fc7559b12066d4c</anchor>
      <arglist>(uint8_t r, uint8_t g, uint8_t b)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_system_init</name>
      <anchorfile>led__task__simple_8h.html</anchorfile>
      <anchor>a7bbc66c7f8801283fece40b2e083d7f5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>led_system_is_initialized</name>
      <anchorfile>led__task__simple_8h.html</anchorfile>
      <anchor>a04e4428b02d2b0776670b98802395a84</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>led_get_color</name>
      <anchorfile>led__task__simple_8h.html</anchorfile>
      <anchor>a4ada7a56726ed5c0f46cdb989c4dcb4f</anchor>
      <arglist>(uint8_t index)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_task_start</name>
      <anchorfile>led__task__simple_8h.html</anchorfile>
      <anchor>a5c2b466a04fcadba97ca90ee811927d8</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_pixel</name>
      <anchorfile>led__task__simple_8h.html</anchorfile>
      <anchor>ada9e83fe87bb057aa60e7bf6d3b23215</anchor>
      <arglist>(uint8_t index, uint8_t r, uint8_t g, uint8_t b)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_clear_all</name>
      <anchorfile>led__task__simple_8h.html</anchorfile>
      <anchor>a323c87898912a4a4d62383dbe8bd7099</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set_all</name>
      <anchorfile>led__task__simple_8h.html</anchorfile>
      <anchor>a914a921ea239786c49543fce7b9fe67a</anchor>
      <arglist>(uint8_t r, uint8_t g, uint8_t b)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>led_unified_api.h</name>
    <path>components/led_task/include/</path>
    <filename>led__unified__api_8h.html</filename>
    <class kind="struct">led_color_t</class>
    <member kind="define">
      <type>#define</type>
      <name>LED_COLOR_OFF</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a2b90b815358f1c78e36a5f4c56308449</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_COLOR_WHITE</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>aa9bb2e0662b7efddf1e12bfaa241b8a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_COLOR_RED</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a8987a69d33df1353af6077ff67dfea28</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_COLOR_GREEN</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a913f289eb3203c4e3043b1ecb4047032</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_COLOR_BLUE</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a3de6c5690933de9d62c0897ef611cb19</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_COLOR_YELLOW</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a97348e8acf253232595a481314f8a396</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_COLOR_CYAN</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a8985352a22885bbb847bca6e772e98bc</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_COLOR_MAGENTA</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a6518de5003bcf7e4853f4dc29e372988</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_set</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a03f1c23d367a417c885fbf0b77fa8e92</anchor>
      <arglist>(uint8_t position, led_color_t color)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_clear</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a738fcd34c4727af46498fbee998f5b27</anchor>
      <arglist>(uint8_t position)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_clear_all</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a323c87898912a4a4d62383dbe8bd7099</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_clear_board</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a489fc3062159f186eaf3110bb9448900</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_show_moves</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a746d846a8984178a6d25821b2485ff38</anchor>
      <arglist>(const uint8_t *positions, uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_show_movable_pieces</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>add8f4af4ac4cd55c7f080b6a0e74c715</anchor>
      <arglist>(const uint8_t *positions, uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_animate</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a1c171299071c1e2c7d6c0744279ea3e7</anchor>
      <arglist>(const char *type, uint8_t from_pos, uint8_t to_pos)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_stop_animations</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>abb65c57a4635e4911d71902ef3bcbb3c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_show_error</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a7dcadeeb97f7284b327666232439764a</anchor>
      <arglist>(uint8_t position)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_show_selection</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a49f0adcc708a2e5b8c5f665fa7e35a0e</anchor>
      <arglist>(uint8_t position)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_show_capture</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a323f5cd41bfd6e9ffa8c0e7a22a4f728</anchor>
      <arglist>(uint8_t position)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>led_commit</name>
      <anchorfile>led__unified__api_8h.html</anchorfile>
      <anchor>a036b1d0a0cf1289a8eec759f01b07233</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>led_task.c</name>
    <path>components/led_task/</path>
    <filename>led__task_8c.html</filename>
    <includes id="led__task_8h" name="led_task.h" local="yes" import="no" module="no" objc="no">led_task.h</includes>
    <includes id="config__manager_8h" name="config_manager.h" local="yes" import="no" module="no" objc="no">../config_manager/include/config_manager.h</includes>
    <includes id="chess__types_8h" name="chess_types.h" local="yes" import="no" module="no" objc="no">../freertos_chess/include/chess_types.h</includes>
    <includes id="streaming__output_8h" name="streaming_output.h" local="yes" import="no" module="no" objc="no">../freertos_chess/include/streaming_output.h</includes>
    <includes id="game__task_8h" name="game_task.h" local="yes" import="no" module="no" objc="no">../game_task/include/game_task.h</includes>
    <includes id="unified__animation__manager_8h" name="unified_animation_manager.h" local="yes" import="no" module="no" objc="no">../unified_animation_manager/include/unified_animation_manager.h</includes>
    <includes id="led__mapping_8h" name="led_mapping.h" local="yes" import="no" module="no" objc="no">led_mapping.h</includes>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <class kind="struct">led_duration_state_t</class>
    <class kind="struct">led_frame_buffer_t</class>
    <class kind="struct">led_health_stats_t</class>
    <class kind="struct">endgame_wave_state_t</class>
    <member kind="define">
      <type>#define</type>
      <name>min</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ac6afabdc09a49a433ee19d8a9486056d</anchor>
      <arglist>(a, b)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>M_PI</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ae71449b1cc6e6250b91f539153a7a0d3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_CRITICAL_SECTION_TIMEOUT_MS</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>adc1b5df15af7552352dad465c61cfac7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_TASK_MUTEX_TIMEOUT_MS</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a0256c8c9fa0cc7b33ef3d2ceb0abb010</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_TASK_MUTEX_TIMEOUT_TICKS</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>acd37c9dc96b5ed1245631969b5daa3f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_FRAME_BUFFER_SIZE</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>af505353c4adf955dea4a330d4177d46c</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_MAX_RETRY_COUNT</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>afb5d542d83382d217381d437d8c1fc73</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_ERROR_RECOVERY_THRESHOLD</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a038bfe7aea2d3849e2ee6a6635b318bc</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_HEALTH_CHECK_INTERVAL_MS</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a9d3a3936c730816cea250929a3ff185a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_BATCH_COMMIT_INTERVAL_MS</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a2e7aa4b575605ebe8928ddc7f944c4b5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_WATCHDOG_RESET_INTERVAL</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a1971e04ffd5581d8847ad715204854d7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LED_DATA_PIN</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a5f55b07707df2f2cf371f707207ed508</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>led_task_wdt_reset_safe</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a3aea89dfd9371256c48be13c636f86e2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>led_send_status_to_uart_immediate</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a330c04b492b83fd0e7c5d7e85d6d845d</anchor>
      <arglist>(QueueHandle_t response_queue)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_execute_command_new</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a85578cf6b1c0acb68c6278c9cd1a4f26</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>led_hardware_init</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a7287362da29fee32e79da1689bca9e31</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>led_hardware_cleanup</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a8aad97d4a7c8e1ab13920b16a6397283</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>led_commit_pending_changes</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>af39bda75c597894d99ae8a5a35542fe9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_force_immediate_update</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a35ed753e036a2372ea57df822d92f154</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>led_privileged_batch_commit</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>aec926993bbb0b624462704e37829a5ab</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_clear_board_only</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ab245ceabb580934b6117f59343c6d9a2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_clear_buttons_only</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a4f7ff7068535f3818fb37654f5bffdad</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_preserve_buttons</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a4653a6179cbaf07289307ce6c1871858</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>led_set_pixel_with_duration</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a8920462d21f5bbd3198dc4cc485d1e82</anchor>
      <arglist>(uint8_t led_index, uint8_t r, uint8_t g, uint8_t b, uint32_t duration_ms)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>led_process_duration_expirations</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>adb4079e18c88be33df2ed464c968fad7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>led_init_duration_system</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a11714f15d535467c7c6011b5510197f5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const char *</type>
      <name>get_ansi_color_from_rgb</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>adab4e0aa5871161ca4d0f13c4c2e9f63</anchor>
      <arglist>(uint8_t r, uint8_t g, uint8_t b)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>led_update_button_led_state</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ab1c29de873addfa75777d115ab4f8438</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>led_set_button_pressed</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a2d9017d1898113d1763fea0e43cc3bd9</anchor>
      <arglist>(uint8_t button_id, bool pressed)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>led_process_button_blink_timers</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a2135e4b3d98992bec7601350d477abc9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_brightness_global</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>aba7d182926787e311733fb41353c196e</anchor>
      <arglist>(uint8_t brightness)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static uint8_t</type>
      <name>led_get_button_led_index</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>abd19bd00f68dac2956a019c2981912e3</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_pixel_internal</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a7cb7bb0774dead39c3607d19de9f9d34</anchor>
      <arglist>(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_all_internal</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a6d5be6d6368f979daf99dea8b1685f55</anchor>
      <arglist>(uint8_t red, uint8_t green, uint8_t blue)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_clear_all_internal</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a50360691268da244abb2711dc712bd54</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_show_chess_board</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a13da231e8e3dc956e4c3415a89409670</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_ha_color</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>acd139d9f40454ed76aa1f080f2299d07</anchor>
      <arglist>(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_restore_chess_board</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>aed864c4fd31d53e8498014ffda17dcc6</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_refresh_all_button_leds</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>acffe67fadfe40ddc4c14618edebeedf8</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_button_feedback</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>aa74e5b3b49114f8e7f69f346beaf5cd3</anchor>
      <arglist>(uint8_t button_id, bool available)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_button_press</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a8a31606dca399a00351e3ef16cc3a737</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_button_release</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a8c05ca5afd0c6a5184c88ee8e05f37b0</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>led_get_button_color</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a949ef87b771a33a9998649a33db6848d</anchor>
      <arglist>(uint8_t button_id)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>led_get_led_state</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a444a170336ec287da201e2899805812b</anchor>
      <arglist>(uint8_t led_index)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_get_all_states</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ab7562384ca3de074cd4f13eb8e96c790</anchor>
      <arglist>(uint32_t *states, size_t max_count)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_start_animation</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a571ae664c39755862a8b153447841fe3</anchor>
      <arglist>(uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_test_pattern</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a909c49ae2d15c636755d044ad2380a74</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_test_all_pattern</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a0ceea2caca51dbc9f19df054f157a6ce</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_process_commands</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a07ea6d48b1deb7fd8ecb18cf6d7753be</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_anim_player_change</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a93931e2c6d0f52d99011f95372bdc495</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_anim_move_path</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a70ca9a0461dd8a73f47f0da3e9d6cc6f</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_anim_castle</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a4c6b6336fa1a5b5e4b11287573e29445</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_anim_promote</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ae9ca69874a3fd0ef1b1aea542f0ce0cf</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_anim_endgame</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a69379eed351dd045d9d23cda7ec86a69</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_update_endgame_wave</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a0d7ed5f284182a8b0bf27f9fc2537445</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_anim_check</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a0eaa8914041f19a17236659faf64f3b9</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_anim_checkmate</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>acf6193320f934a4b77f6e295d2cc946a</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_enhanced_castling_guidance</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ae1107d12af4474cf714958ea8078de67</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_enhanced_castling_error</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>af3edc8a3c773252ef88840e95dafe1b6</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_enhanced_castling_celebration</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ab3b1b195cc27c98b66f82c0887cc425b</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_enhanced_castling_tutorial</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a16442a4b2c22fe558767b11e5252beae</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_enhanced_castling_clear</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a419b696d044d30102e185f17071d5594</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_update_hardware</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ae35498b645fad1892ac45a425bf78cce</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_update_animation</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a44d83bc10edfd5ea26219455ee937489</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_print_compact_status</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a4f270f2071fdb90dc6da3d8d6d0e7fe2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_print_detailed_status</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a57338ebc4262ba916e9a68acdc8634fa</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_update_game_state</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>af7a7e8b554370642f118c98fd1015e86</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_highlight_pieces_that_can_move</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a320f9c61a3b7d32d1244558bc94affd4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_highlight_possible_moves</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a54ef8b8e4b3a77b63a1968de1f488759</anchor>
      <arglist>(uint8_t from_square)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_clear_all_highlights</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a3f8208e54db356a3a750b566d5bd763f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_player_change_animation</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a7e000dafeeac5016d70d5d182b89c10f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_print_changes_only</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a39e8c14550a437ae7b94b56d8583b27a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_start_quiet_period</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ad7200d35f2087aa4d23aa18fb79e87a5</anchor>
      <arglist>(uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>led_has_significant_changes</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a13a1293f8ac28cbf7cd0d7b08cb58c5c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_task_start</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a5c2b466a04fcadba97ca90ee811927d8</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>led_is_booting</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a006a24ec8e7f302c4a6f8481675803d1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_booting_animation</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a533dfd80b93f0821af12a5f929dcc40b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_stop_endgame_animation</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a081382ca7f7a773156291851fdbe6f5f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_setup_animation_after_endgame</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a38353130f2210565c3c71c96f05ecf04</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_pixel_safe</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ad4b7ef962f86de71ef5f934f2a92544a</anchor>
      <arglist>(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_clear_all_safe</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>afff280b4c9b7261d1a086e0cfa356604</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_all_safe</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a67410e7a666d076402f2eaf116716a2b</anchor>
      <arglist>(uint8_t red, uint8_t green, uint8_t blue)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_show_legal_moves</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a473fe800a9a37ba1caee73edd0398505</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_error_invalid_move</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a1a82f432b46c4990df10c3c8c4d697b4</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_error_return_piece</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a5fed0119b12c12bf3cee449144e40d43</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_error_recovery</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a8811aae10a60d7851edfc7af2ef69771</anchor>
      <arglist>(const led_command_t *cmd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_set_button_promotion_available</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ac30980ab24cfbd6fe5684d6ade1c1f7a</anchor>
      <arglist>(uint8_t button_id, bool available)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_update_button_availability_from_game</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a31b820a4bfe33d0cf54a107c339d6d07</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_boot_animation_step</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ad9333c97e3a0d22851f18f91e890e3dc</anchor>
      <arglist>(uint8_t progress_percent)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>led_boot_animation_fade_out</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a51cc23ac460421224cfaf847669ab1d9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>led_states</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a3a21d6fc11052b5e1badc86408a5f3b2</anchor>
      <arglist>[(64+9)]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>button_pressed</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ad46716f57f8840bf466f5182b422923d</anchor>
      <arglist>[9]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>button_available</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ab1b272f31f77f519b306256237a53f62</anchor>
      <arglist>[9]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>button_release_time</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a4d326bc74da8f9d9728f85a85bb098b6</anchor>
      <arglist>[9]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>button_blinking</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a5b29e2f7276c89b3c3dd9d870dbc0245</anchor>
      <arglist>[9]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>animation_active</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a865a894de09d6963ba453a60b2222bba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>animation_start_time</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a381f77567d8c6cc3dcc0cadc9b3ad5ca</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>animation_duration</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a0d754cbffa8c8c66e7dec18f65474c99</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>animation_pattern</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ae7807e34a9e000cf160c7cbdea67089b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>task_running</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a35141cb45623cb3b7b4d96da35ed1831</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>matrix_scanning_enabled</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ac4220888456a96a1308bbf06df3730b8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>led_component_enabled</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a8bfc7db202490dafea98d0ede64bc881</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>simulation_mode</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a740a5ff73f18495856ddbc99ad099a6b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static led_strip_handle_t</type>
      <name>led_strip</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>abbc5a11f432a69a8fc4d2798e9302d6f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>led_initialized</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>aa059cb143b37ad593835d4042adf415b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static SemaphoreHandle_t</type>
      <name>led_unified_mutex</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>abf08412fd7527bf3e406512943ab8ec7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>led_changes_pending</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ad291c3f82f7856c496990a2ecfe5da23</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>led_pending_changes</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a42d3ab57814906b861bdbdba7063e20b</anchor>
      <arglist>[(64+9)]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>led_changed_flags</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ac1c48ef4ad2c752b3812d8e4e3e5324c</anchor>
      <arglist>[(64+9)]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static led_duration_state_t</type>
      <name>led_durations</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a0e1b74e72cc66b31800fb54c57a08fb3</anchor>
      <arglist>[(64+9)]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>led_duration_system_enabled</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ab5eca3f51d48f047dfa62384299a1b6f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>global_brightness</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ac96aaee8c56d42372878aa20a24362c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const uint32_t</type>
      <name>chess_board_pattern</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a7ae8f1d599ba588ecd3a3709345bb7ee</anchor>
      <arglist>[64]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const uint32_t</type>
      <name>test_pattern_colors</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ad0b9fb10f5bbede6ffb70c90f0b283cd</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>previous_led_states</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a167c892c66569d146d1f3aebabf1cabb</anchor>
      <arglist>[(64+9)]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>led_states_initialized</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a0b78e63c2dfab80c964d3d5fe1453985</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>last_status_report_time</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ab30ee6f7006cc4b6a64187bcc4271dec</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>led_changes_count</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a10bd2320bc021e09d3a1e6f889617725</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>quiet_period_active</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a97a595620422ca75117a4de4f2bc0b99</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>quiet_period_start</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>ac888595342bcfbaf64cb95f8c4272045</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>quiet_period_duration</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a5c2607d09b8412f0a6484a21095b260f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>color_names</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a95fe2d6cbd671926e57533891a3b8cac</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>button_names</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a5a8451e4ed607d6e653e39c1122ae600</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>endgame_animation_active</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a7c488b223b05495652330bf542a90669</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>led_booting_active</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a7005076d8f4d04f878fd4c8e3efdcce5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static endgame_wave_state_t</type>
      <name>endgame_wave</name>
      <anchorfile>led__task_8c.html</anchorfile>
      <anchor>a1ed1a514affa226ab201a5e7334e32a7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>matrix_task.h</name>
    <path>components/matrix_task/include/</path>
    <filename>matrix__task_8h.html</filename>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <member kind="function">
      <type>void</type>
      <name>matrix_task_start</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>a61e7f4685d87d40d712b5de9b8d22222</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_scan_row</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>a395c914a55ec3ebc25a3a3af66d6bba8</anchor>
      <arglist>(uint8_t row)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_scan_all</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>a86518ffc745875b5c3c34d7c96f7e186</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_detect_moves</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>ad81a1142e821938e81029ad1d88f57d7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_detect_complete_move</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>a9f313f74d8c7d9bac02294a7f873eb75</anchor>
      <arglist>(uint8_t from_square, uint8_t to_square)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_square_to_notation</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>ab2ee938ff9e39dbaeed9729d194db94a</anchor>
      <arglist>(uint8_t square, char *notation)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>matrix_notation_to_square</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>a7401a07900074895370aa1ba67a95c56</anchor>
      <arglist>(const char *notation)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_print_state</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>a4879f19808c12282ff427342dfea0f12</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_simulate_move</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>ac791c32154297e3ceebd47d8096cc8b9</anchor>
      <arglist>(const char *from, const char *to)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_get_state</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>a62798e0c14260b016d8d1739dedc2056</anchor>
      <arglist>(uint8_t *state_buffer)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_process_commands</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>a60eff3fc23741f64ce1c24d5d556906b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_reset</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>aa7161ac2bf0076cc5ddfc91270c2bd34</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_release_pins</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>a9afd14fa75a6cfa6e37480b4935cd285</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_acquire_pins</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>a8162927b4230f6777b1d42deec017797</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>matrix_pins_released</name>
      <anchorfile>matrix__task_8h.html</anchorfile>
      <anchor>a4ffeb65e7984e50f1ad65b6a92c304cf</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>matrix_task.c</name>
    <path>components/matrix_task/</path>
    <filename>matrix__task_8c.html</filename>
    <includes id="matrix__task_8h" name="matrix_task.h" local="yes" import="no" module="no" objc="no">matrix_task.h</includes>
    <includes id="ha__light__task_8h" name="ha_light_task.h" local="yes" import="no" module="no" objc="no">../ha_light_task/include/ha_light_task.h</includes>
    <includes id="chess__types_8h" name="chess_types.h" local="yes" import="no" module="no" objc="no">chess_types.h</includes>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <member kind="function">
      <type>void</type>
      <name>demo_report_activity</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a768b8bf362b42a4e04ac58639035a938</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>matrix_task_wdt_reset_safe</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>ad3ad65033f5eaaf1b607d806e212fe50</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>matrix_scan_row_internal</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a2a57a5bda73ab9817a44c3c5702932f3</anchor>
      <arglist>(uint8_t row)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_scan_row</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a395c914a55ec3ebc25a3a3af66d6bba8</anchor>
      <arglist>(uint8_t row)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_scan_all</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a86518ffc745875b5c3c34d7c96f7e186</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>matrix_send_pickup_command</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>ae622813bcd8b41c1040db3a6c8d34e13</anchor>
      <arglist>(uint8_t square)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>matrix_send_drop_command_with_from</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a275f664c60af320845973b221bd1db55</anchor>
      <arglist>(uint8_t from_square, uint8_t to_square)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_detect_moves</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>ad81a1142e821938e81029ad1d88f57d7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_detect_complete_move</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a9f313f74d8c7d9bac02294a7f873eb75</anchor>
      <arglist>(uint8_t from_square, uint8_t to_square)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_square_to_notation</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>ab2ee938ff9e39dbaeed9729d194db94a</anchor>
      <arglist>(uint8_t square, char *notation)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>matrix_notation_to_square</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a7401a07900074895370aa1ba67a95c56</anchor>
      <arglist>(const char *notation)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_get_state</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a62798e0c14260b016d8d1739dedc2056</anchor>
      <arglist>(uint8_t *state_buffer)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_print_state</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a4879f19808c12282ff427342dfea0f12</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_test_scanning</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>af455a307ac1bd0e33a3c213d3a5889e3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_simulate_move</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>ac791c32154297e3ceebd47d8096cc8b9</anchor>
      <arglist>(const char *from, const char *to)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_process_commands</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a60eff3fc23741f64ce1c24d5d556906b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_reset</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>aa7161ac2bf0076cc5ddfc91270c2bd34</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_release_pins</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a9afd14fa75a6cfa6e37480b4935cd285</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_acquire_pins</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a8162927b4230f6777b1d42deec017797</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>matrix_pins_released</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a4ffeb65e7984e50f1ad65b6a92c304cf</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>matrix_task_start</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a61e7f4685d87d40d712b5de9b8d22222</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>matrix_state</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a3a1805bac344d932f67d863f671240d1</anchor>
      <arglist>[64]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>matrix_previous</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>add49e624d0cc3921ffa016066be0f462</anchor>
      <arglist>[64]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>matrix_changes</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>aa6b75ccca141dbb71ab2e190fc56078b</anchor>
      <arglist>[64]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>task_running</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a35141cb45623cb3b7b4d96da35ed1831</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>simulation_mode</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a740a5ff73f18495856ddbc99ad099a6b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>matrix_scanning_enabled</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>ac4220888456a96a1308bbf06df3730b8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>current_row</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>aeccb1670596f160cce0ab5caec367e05</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>last_scan_time</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a830cb4e3e4e340d70981f59ced6381fd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>scan_count</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>aadac7bd6fb2efeee4da9a5432123db70</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>last_piece_lifted</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a25428bbb3212a5e17896b6f0de83ca2f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>last_piece_placed</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a41dfccc3e986ded30a04de8de37d251f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>move_detection_timeout</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a001592c98e5a19d78635751717390918</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const uint8_t</type>
      <name>simulation_patterns</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>abc6d07b73f387c902d6c85c33e923cfa</anchor>
      <arglist>[][64]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>current_pattern</name>
      <anchorfile>matrix__task_8c.html</anchorfile>
      <anchor>a859ec6ecf084e6271c6b99fb28f6be94</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>promotion_button_task.h</name>
    <path>components/promotion_button_task/include/</path>
    <filename>promotion__button__task_8h.html</filename>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>PROMOTION_BUTTON_TASK_PRIORITY</name>
      <anchorfile>promotion__button__task_8h.html</anchorfile>
      <anchor>a58d022d18b73ee9efae31b39b6fd4343</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>promotion_button_task_init</name>
      <anchorfile>promotion__button__task_8h.html</anchorfile>
      <anchor>a4a7c298b8b954ba4e407167ec242242b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>promotion_button_task</name>
      <anchorfile>promotion__button__task_8h.html</anchorfile>
      <anchor>a31932e440404a48bbeee15bd30220f31</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>process_promotion_choice</name>
      <anchorfile>promotion__button__task_8h.html</anchorfile>
      <anchor>a8e3a45fc9ac9fcda6e243449b9de66ff</anchor>
      <arglist>(promotion_choice_t choice)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>simulate_promotion_button_press</name>
      <anchorfile>promotion__button__task_8h.html</anchorfile>
      <anchor>a0e0d52d07d70caef22af40a2969e79c3</anchor>
      <arglist>(uint8_t button_index)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>promotion_button_is_initialized</name>
      <anchorfile>promotion__button__task_8h.html</anchorfile>
      <anchor>af6e9bc64868813a9a0c6134a471aefc9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>promotion_button_get_event_count</name>
      <anchorfile>promotion__button__task_8h.html</anchorfile>
      <anchor>acc92043a584171d17f29d9e0ae9bbd1a</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>promotion_button_task.c</name>
    <path>components/promotion_button_task/</path>
    <filename>promotion__button__task_8c.html</filename>
    <includes id="promotion__button__task_8h" name="promotion_button_task.h" local="yes" import="no" module="no" objc="no">promotion_button_task.h</includes>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <member kind="function">
      <type>esp_err_t</type>
      <name>promotion_button_task_init</name>
      <anchorfile>promotion__button__task_8c.html</anchorfile>
      <anchor>a4a7c298b8b954ba4e407167ec242242b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>promotion_button_task</name>
      <anchorfile>promotion__button__task_8c.html</anchorfile>
      <anchor>a31932e440404a48bbeee15bd30220f31</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>process_promotion_choice</name>
      <anchorfile>promotion__button__task_8c.html</anchorfile>
      <anchor>a8e3a45fc9ac9fcda6e243449b9de66ff</anchor>
      <arglist>(promotion_choice_t choice)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>simulate_promotion_button_press</name>
      <anchorfile>promotion__button__task_8c.html</anchorfile>
      <anchor>a0e0d52d07d70caef22af40a2969e79c3</anchor>
      <arglist>(uint8_t button_index)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>promotion_button_is_initialized</name>
      <anchorfile>promotion__button__task_8c.html</anchorfile>
      <anchor>af6e9bc64868813a9a0c6134a471aefc9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>promotion_button_get_event_count</name>
      <anchorfile>promotion__button__task_8c.html</anchorfile>
      <anchor>acc92043a584171d17f29d9e0ae9bbd1a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>promotion__button__task_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>promotion_button_initialized</name>
      <anchorfile>promotion__button__task_8c.html</anchorfile>
      <anchor>abbaaee3316ba8718fb74559d7c93df9b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>button_event_count</name>
      <anchorfile>promotion__button__task_8c.html</anchorfile>
      <anchor>ab36b66e9f95206af3b83dfa622d5cc7d</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>reset_button_task.h</name>
    <path>components/reset_button_task/include/</path>
    <filename>reset__button__task_8h.html</filename>
    <member kind="function">
      <type>esp_err_t</type>
      <name>reset_button_task_init</name>
      <anchorfile>reset__button__task_8h.html</anchorfile>
      <anchor>adf092b27f7d985c1b6e1f5980a1ec99e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>reset_button_task</name>
      <anchorfile>reset__button__task_8h.html</anchorfile>
      <anchor>a2b2018c335a91336642118cc74af7b4a</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>process_reset_request</name>
      <anchorfile>reset__button__task_8h.html</anchorfile>
      <anchor>a828ddd0ce9a37f1fbe484e6663d96a57</anchor>
      <arglist>(bool reset_request)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>simulate_reset_button_press</name>
      <anchorfile>reset__button__task_8h.html</anchorfile>
      <anchor>a9d96f74aeb8c8ab64a9ea78a857747dc</anchor>
      <arglist>(bool pressed)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>reset_button_is_initialized</name>
      <anchorfile>reset__button__task_8h.html</anchorfile>
      <anchor>a9c119f00a30675bdd20fd69a03775b71</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>reset_button_get_event_count</name>
      <anchorfile>reset__button__task_8h.html</anchorfile>
      <anchor>a06d02f46f762a840863c8353f7e7f506</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>reset_button_task.c</name>
    <path>components/reset_button_task/</path>
    <filename>reset__button__task_8c.html</filename>
    <includes id="reset__button__task_8h" name="reset_button_task.h" local="yes" import="no" module="no" objc="no">reset_button_task.h</includes>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <member kind="function">
      <type>esp_err_t</type>
      <name>reset_button_task_init</name>
      <anchorfile>reset__button__task_8c.html</anchorfile>
      <anchor>adf092b27f7d985c1b6e1f5980a1ec99e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>reset_button_task</name>
      <anchorfile>reset__button__task_8c.html</anchorfile>
      <anchor>a2b2018c335a91336642118cc74af7b4a</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>process_reset_request</name>
      <anchorfile>reset__button__task_8c.html</anchorfile>
      <anchor>a828ddd0ce9a37f1fbe484e6663d96a57</anchor>
      <arglist>(bool reset_request)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>simulate_reset_button_press</name>
      <anchorfile>reset__button__task_8c.html</anchorfile>
      <anchor>a9d96f74aeb8c8ab64a9ea78a857747dc</anchor>
      <arglist>(bool pressed)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>reset_button_is_initialized</name>
      <anchorfile>reset__button__task_8c.html</anchorfile>
      <anchor>a9c119f00a30675bdd20fd69a03775b71</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>reset_button_get_event_count</name>
      <anchorfile>reset__button__task_8c.html</anchorfile>
      <anchor>a06d02f46f762a840863c8353f7e7f506</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>reset__button__task_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>reset_button_initialized</name>
      <anchorfile>reset__button__task_8c.html</anchorfile>
      <anchor>a2a86e8e816b985c84e78ba6b842e52bf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>button_event_count</name>
      <anchorfile>reset__button__task_8c.html</anchorfile>
      <anchor>ab36b66e9f95206af3b83dfa622d5cc7d</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>test_task.h</name>
    <path>components/test_task/include/</path>
    <filename>test__task_8h.html</filename>
    <member kind="enumeration">
      <type></type>
      <name>test_result_t</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a2682e047750c295771c3a6953c8393e1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TEST_RESULT_PASS</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a2682e047750c295771c3a6953c8393e1a5915015357cf50b371e59de2436faa63</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TEST_RESULT_FAIL</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a2682e047750c295771c3a6953c8393e1ae705a406cd468034f29e2cfe794071bd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TEST_RESULT_SKIP</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a2682e047750c295771c3a6953c8393e1a382b2558359637c37d7938164a9cf26a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TEST_RESULT_ERROR</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a2682e047750c295771c3a6953c8393e1ab5b36dc7d973cf3a228125b15daf73c3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>test_command_type_t</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a113ea1691ec5f80092fe8a21515419a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TEST_CMD_RUN_ALL</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a113ea1691ec5f80092fe8a21515419a8adfd866aa3fef19121adc7b24a8da4d6c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TEST_CMD_RUN_SUITE</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a113ea1691ec5f80092fe8a21515419a8adf3377b5d435c283adf381d17aa80abc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TEST_CMD_RUN_SINGLE</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a113ea1691ec5f80092fe8a21515419a8a44e37fcf48b4f55fa5cc4abc7b44011f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TEST_CMD_GET_STATUS</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a113ea1691ec5f80092fe8a21515419a8affeed03e7ecbbefc776a8944a498f0ad</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_task_start</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>af82dfdecbc2c7932876c821753490930</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_initialize_system</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a521eb2232a4f3086cec73dd1de28babd</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>test_create_suite</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>aa7cd14248b427bac3a37062c096666fe</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_add_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a8b2798a528f6bc49d7c66167e4ea7441</anchor>
      <arglist>(uint8_t suite_id, const char *name, bool enabled)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_add_hardware_tests</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>aaa28933bc8b8d3ca51e6a710bd02a1c0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_add_system_tests</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a1f2ffae04cabc924e27e6a4afe2b51e2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_add_performance_tests</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a025f24a6b65de4002a1ad3a71f5285e2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_add_integration_tests</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>af7d9d5c0f04a11d687b04aeb1f3f81a5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_run_all_suites</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>ace48e52c97a70560c37d0df5861549b2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_run_suite</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a5b116c2efc3cea334eee3f42cdc03a0e</anchor>
      <arglist>(uint8_t suite_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_run_single_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>abce606bebe3aab6e9cd5bb6210d28d70</anchor>
      <arglist>(uint8_t suite_id, uint8_t test_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_complete_all_suites</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>afbb84e98527e95e28ef42e3b0b038895</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_led_matrix_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a04254feb3658e3213b535c2692ba3468</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_button_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>aefcd5d044060161ae93bb8278e88e66a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_gpio_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>ac3e463913eb21836ad10b3086db30637</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_ws2812b_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a50fc456ac618476d01505b4b09cd6fa1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_reed_switch_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>aea5591c0f06150307252fc7a9757cf06</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_power_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>ac078516cdae5c2a8616a70773c5b82fd</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_clock_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a3377430330f2ea8ff39fb88e8a1cde8c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_memory_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a720c34a2f3e433cd6c1b652bfd685dc2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_freertos_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a69c9f697703db2ee96da83e7c09c9780</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_queue_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>ae837e8f043dd392abcdc36f4629ecab4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_mutex_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>aaac45356aafa3772ca815cfabde4ebe3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_timer_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a0c3649af88997796d01a101a744cbb2d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_interrupt_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>afbb6c7d72aaea601299f309155ce7512</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_error_handling_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a3bff828ce05f71a35ce5e7b77e2eff8c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_logging_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a6270fd9213f50873c81cd9bad250e25a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_configuration_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>aead619aa942e29bd1f458e1e8c9e11de</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_performance_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a683341890d9ac3d251c643c6577c745d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_integration_test</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>ad67106393df1a1534e27a2577a8bb33b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_process_commands</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a206b14c593b5b749fa9f00b346cbbf39</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_print_status</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a30a2be5d905ed5d99cb45aa9d74d4416</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_print_detailed_results</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a9e321b54269fca785bff5da1f6c1c972</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_reset_results</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>ac2da45140d1e39b72d5f585c1f1c14c6</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_run_performance_benchmark</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>ade8ae2c431fa32bffcffc266a37c71f8</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>test_command_queue</name>
      <anchorfile>test__task_8h.html</anchorfile>
      <anchor>a849df7d33a2eeeb8383dab84223a7813</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>test_task.c</name>
    <path>components/test_task/</path>
    <filename>test__task_8c.html</filename>
    <includes id="test__task_8h" name="test_task.h" local="yes" import="no" module="no" objc="no">test_task.h</includes>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <includes id="led__task__simple_8h" name="led_task_simple.h" local="yes" import="no" module="no" objc="no">led_task_simple.h</includes>
    <class kind="struct">test_t</class>
    <class kind="struct">test_suite_t</class>
    <member kind="define">
      <type>#define</type>
      <name>MAX_TEST_SUITES</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a1b26c6ce8cf1a1095f62b5a1b5162c9f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX_TESTS_PER_SUITE</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a3d48460cefc49b6243ab314dd001930c</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>TEST_TIMEOUT_MS</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a90fcbca855c6c141b3f213a229c7016a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>TEST_TASK_INTERVAL</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>af304591824ffd0303612bc129b97474b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>test_state_t</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a771b98f001ed3e41e779472ee7cedc33</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TEST_STATE_IDLE</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a771b98f001ed3e41e779472ee7cedc33a36d50de8eedc057e582fd554d029f437</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TEST_STATE_RUNNING</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a771b98f001ed3e41e779472ee7cedc33aec9f23523284505bdffa96de4c2b05e4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TEST_STATE_PAUSED</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a771b98f001ed3e41e779472ee7cedc33a23c6a6bb17273b0f5bce7cd89c970199</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TEST_STATE_COMPLETED</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a771b98f001ed3e41e779472ee7cedc33ade5410e738d2a5d51d3b50b5cc65e68b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TEST_STATE_FAILED</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a771b98f001ed3e41e779472ee7cedc33a2ba728ab76fb4ef3eda879710a964f5f</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>test_task_wdt_reset_safe</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>ac21b25ddf3b1c4cace1096ac1767b6fa</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_initialize_system</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a521eb2232a4f3086cec73dd1de28babd</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>test_create_suite</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>aa7cd14248b427bac3a37062c096666fe</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_add_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a8b2798a528f6bc49d7c66167e4ea7441</anchor>
      <arglist>(uint8_t suite_id, const char *name, bool enabled)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_add_hardware_tests</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>aaa28933bc8b8d3ca51e6a710bd02a1c0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_add_system_tests</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a1f2ffae04cabc924e27e6a4afe2b51e2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_add_performance_tests</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a025f24a6b65de4002a1ad3a71f5285e2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_add_integration_tests</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>af7d9d5c0f04a11d687b04aeb1f3f81a5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_run_all_suites</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>ace48e52c97a70560c37d0df5861549b2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_run_suite</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a5b116c2efc3cea334eee3f42cdc03a0e</anchor>
      <arglist>(uint8_t suite_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_run_single_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>abce606bebe3aab6e9cd5bb6210d28d70</anchor>
      <arglist>(uint8_t suite_id, uint8_t test_id)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_led_matrix_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a04254feb3658e3213b535c2692ba3468</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_button_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>aefcd5d044060161ae93bb8278e88e66a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_gpio_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>ac3e463913eb21836ad10b3086db30637</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_ws2812b_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a50fc456ac618476d01505b4b09cd6fa1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_reed_switch_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>aea5591c0f06150307252fc7a9757cf06</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_power_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>ac078516cdae5c2a8616a70773c5b82fd</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_clock_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a3377430330f2ea8ff39fb88e8a1cde8c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_memory_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a720c34a2f3e433cd6c1b652bfd685dc2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_freertos_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a69c9f697703db2ee96da83e7c09c9780</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_queue_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>ae837e8f043dd392abcdc36f4629ecab4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_mutex_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>aaac45356aafa3772ca815cfabde4ebe3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_timer_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a0c3649af88997796d01a101a744cbb2d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_interrupt_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>afbb6c7d72aaea601299f309155ce7512</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_error_handling_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a3bff828ce05f71a35ce5e7b77e2eff8c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_logging_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a6270fd9213f50873c81cd9bad250e25a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_configuration_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>aead619aa942e29bd1f458e1e8c9e11de</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_performance_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a683341890d9ac3d251c643c6577c745d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>test_result_t</type>
      <name>test_execute_integration_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>ad67106393df1a1534e27a2577a8bb33b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_complete_all_suites</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>afbb84e98527e95e28ef42e3b0b038895</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_process_commands</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a206b14c593b5b749fa9f00b346cbbf39</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_print_status</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a30a2be5d905ed5d99cb45aa9d74d4416</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_print_detailed_results</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a9e321b54269fca785bff5da1f6c1c972</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_reset_results</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>ac2da45140d1e39b72d5f585c1f1c14c6</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_run_performance_benchmark</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>ade8ae2c431fa32bffcffc266a37c71f8</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>test_task_start</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>af82dfdecbc2c7932876c821753490930</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>task_running</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a35141cb45623cb3b7b4d96da35ed1831</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static test_state_t</type>
      <name>current_test_state</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a774d23f220a18e0e5b416d8b85eb4ec7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static test_suite_t</type>
      <name>test_suites</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>acec4d1516456c4271605060d93291c76</anchor>
      <arglist>[10]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>suite_count</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a6ed118a5cdded2581063d5bb25cf5d82</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>current_suite</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a621746122b4e1085fc8caaa6b0af5436</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>current_test</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a5124dc52ed577e8d54bbf4615c8c51cb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>total_tests</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>add8bc12c34e62d7b7773b944bd31d930</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>total_passed</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a665695309d1154cdfcfadf03d385becd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>total_failed</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a1bc82f4861d2c36911901018ed4b3a6b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>total_skipped</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>aa6c3ed1f26f0f8ecbd08610ccc6a69c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>test_start_time</name>
      <anchorfile>test__task_8c.html</anchorfile>
      <anchor>a7197f18ba1c3fef844e8d56fc3d132d4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>timer_system.h</name>
    <path>components/timer_system/include/</path>
    <filename>timer__system_8h.html</filename>
    <class kind="struct">time_control_config_t</class>
    <class kind="struct">chess_timer_t</class>
    <member kind="enumeration">
      <type></type>
      <name>time_control_type_t</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_NONE</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97acf683a55dbe135350a71b0ae0fbf10c0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_BULLET_1_0</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97a4590561dd3adc65b92f81fe3f8053621</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_BULLET_1_1</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97abfd7351a76a07e65ec86ca5c746979fb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_BULLET_2_1</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97ac79917867e8f53ebc64e9ad0803e2d10</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_BLITZ_3_0</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97a1c86f38b02477a4ffae67385557b7a0c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_BLITZ_3_2</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97ac39517d993f861ef116e27a65230e36e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_BLITZ_5_0</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97afeb4080afc1298ba5ce62539db577930</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_BLITZ_5_3</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97a2d310c7eb7f901c4a224147c25246b55</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_RAPID_10_0</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97a47f79cf56da828e18a83f0706683ec64</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_RAPID_10_5</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97ac9c5b404dfc4d1201a9079fd5311c550</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_RAPID_15_10</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97a612f0dba840f6eaabbeea7c8dcda66e1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_RAPID_30_0</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97a6518ce7c17b27cb83fd62f9c87b5d8b0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_CLASSICAL_60_0</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97a964a727e4de3eda49c2a016f08abc9bf</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_CLASSICAL_90_30</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97a3a174259dec49ebf339cd25e5bcd68f0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_CUSTOM</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97ad913e6fe488ef15d0d74676c2f09cef0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>TIME_CONTROL_MAX</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a5f46eb643558022bf40c897c5d752f97aa9340059a5efb33278af20922b715b55</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_system_init</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a8ad517573ace8a78289a8305b9acb3ea</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_set_time_control</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a158cdf2baa4bc74c696f82e8c610737d</anchor>
      <arglist>(const time_control_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_start_move</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a23265b63c17ea7990a7362a4d497d6d9</anchor>
      <arglist>(bool is_white_turn)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_end_move</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a89c7235662a4eeb08b1d5e8b77c58492</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_pause</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a66030ee8356a62eed7bbe0717cbd5f22</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_resume</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a48d538badd51ca6eca88b2f54ae3f7fe</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_reset</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>ac52afa2de7a6af69f2f67d86af351a05</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>timer_check_timeout</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>aaa24fcf316f295f7a17bf2ff5ed8dd5e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_get_state</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a10198b60b509490f91eee1685ba0d980</anchor>
      <arglist>(chess_timer_t *timer_data)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>timer_get_remaining_time</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a3129dee6f4627aead1732185abca4395</anchor>
      <arglist>(bool is_white_turn)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_get_config_by_type</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>acc36d1000e481c6d6062135a9f115d40</anchor>
      <arglist>(time_control_type_t type, time_control_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_get_json</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a902602a46b245d091fd3d474ecde0e82</anchor>
      <arglist>(char *buffer, size_t buffer_size)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>timer_get_available_controls_count</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a701d8be89867444c5062b138fda7eaf9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>timer_get_available_controls</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a737397401ab70e21a2979653e14837e6</anchor>
      <arglist>(time_control_config_t *controls, uint32_t max_count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_save_settings</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a3eb1bcce9e90aea183f509abe543f624</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_load_settings</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a8f43d4d19e19f59397bdb2e824e89edc</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>timer_get_average_move_time</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a918dfb50c8da934336dc81cf36c28a08</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>timer_get_total_moves</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a1e23fdb509bdce625a0de3e5ce093fba</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>timer_is_active</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a0812f137aa77d3f66c950c1b98505e20</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>time_control_type_t</type>
      <name>timer_get_current_type</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>af9ad41921ea1477b929f7dd703b71adc</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_set_custom_time_control</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>a11ab1bc968801095ba83fad279c4d282</anchor>
      <arglist>(uint32_t minutes, uint32_t increment_seconds)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_get_config_by_index</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>ab0399771af7d827987018c7153cd381b</anchor>
      <arglist>(uint32_t index, time_control_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_system_deinit</name>
      <anchorfile>timer__system_8h.html</anchorfile>
      <anchor>aeaa8c85da85c6cdb61cf09dc46dd1c64</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>timer_system.c</name>
    <path>components/timer_system/</path>
    <filename>timer__system_8c.html</filename>
    <includes id="timer__system_8h" name="timer_system.h" local="yes" import="no" module="no" objc="no">include/timer_system.h</includes>
    <includes id="led__task_8h" name="led_task.h" local="yes" import="no" module="no" objc="no">../led_task/include/led_task.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>TIMER_NVS_NAMESPACE</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>aa2d35ddf527366525ad43395c5d84a1d</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>timer_lock</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a2564ea8267589d9d9a410ae9660244bc</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>timer_unlock</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>ad91e628b716daa53e3d815833237585f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>timer_update_player_time</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>ad1b18bb2b8d58c81c3745935c4b4566d</anchor>
      <arglist>(bool is_white_turn, uint32_t elapsed_ms)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>timer_check_warnings</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>afa898189615e2a3f5ee1a607879065db</anchor>
      <arglist>(uint32_t time_ms, bool is_white_turn)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>timer_reset_warnings</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>aed96788fc40bba853b0b829fe0a92171</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_system_init</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a8ad517573ace8a78289a8305b9acb3ea</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_set_time_control</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a158cdf2baa4bc74c696f82e8c610737d</anchor>
      <arglist>(const time_control_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_start_move</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a23265b63c17ea7990a7362a4d497d6d9</anchor>
      <arglist>(bool is_white_turn)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_end_move</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a89c7235662a4eeb08b1d5e8b77c58492</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_pause</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a66030ee8356a62eed7bbe0717cbd5f22</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_resume</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a48d538badd51ca6eca88b2f54ae3f7fe</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_reset</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>ac52afa2de7a6af69f2f67d86af351a05</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>timer_check_timeout</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>aaa24fcf316f295f7a17bf2ff5ed8dd5e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_get_state</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a10198b60b509490f91eee1685ba0d980</anchor>
      <arglist>(chess_timer_t *timer_data)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>timer_get_remaining_time</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a3129dee6f4627aead1732185abca4395</anchor>
      <arglist>(bool is_white_turn)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_get_config_by_type</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>acc36d1000e481c6d6062135a9f115d40</anchor>
      <arglist>(time_control_type_t type, time_control_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_get_json</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a902602a46b245d091fd3d474ecde0e82</anchor>
      <arglist>(char *buffer, size_t buffer_size)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>timer_get_available_controls_count</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a701d8be89867444c5062b138fda7eaf9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>timer_get_available_controls</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a737397401ab70e21a2979653e14837e6</anchor>
      <arglist>(time_control_config_t *controls, uint32_t max_count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_save_settings</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a3eb1bcce9e90aea183f509abe543f624</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_load_settings</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a8f43d4d19e19f59397bdb2e824e89edc</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>timer_get_average_move_time</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a918dfb50c8da934336dc81cf36c28a08</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>timer_get_total_moves</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a1e23fdb509bdce625a0de3e5ce093fba</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>timer_is_active</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a0812f137aa77d3f66c950c1b98505e20</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>time_control_type_t</type>
      <name>timer_get_current_type</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>af9ad41921ea1477b929f7dd703b71adc</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_set_custom_time_control</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a11ab1bc968801095ba83fad279c4d282</anchor>
      <arglist>(uint32_t minutes, uint32_t increment_seconds)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_get_config_by_index</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>ab0399771af7d827987018c7153cd381b</anchor>
      <arglist>(uint32_t index, time_control_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>timer_system_deinit</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>aeaa8c85da85c6cdb61cf09dc46dd1c64</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static SemaphoreHandle_t</type>
      <name>timer_mutex</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a82f8e7844502a13352b5d4bd569ade2f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static chess_timer_t</type>
      <name>current_timer</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a790e21ea2404a49eb5a2729720360737</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const time_control_config_t</type>
      <name>TIME_CONTROLS</name>
      <anchorfile>timer__system_8c.html</anchorfile>
      <anchor>a9e1944a686d4ae4557f50f658cb6b03c</anchor>
      <arglist>[]</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>uart_commands_extended.h</name>
    <path>components/uart_commands_extended/include/</path>
    <filename>uart__commands__extended_8h.html</filename>
    <includes id="game__led__animations_8h" name="game_led_animations.h" local="yes" import="no" module="no" objc="no">game_led_animations.h</includes>
    <member kind="function">
      <type>esp_err_t</type>
      <name>register_extended_uart_commands</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>a91f740ce224db3050d1f85890fdbbdce</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>cmd_endgame_animations</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>ace118ff57c1728a13e4660df6234a4a8</anchor>
      <arglist>(int argc, char **argv, char *response, size_t response_size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>cmd_endgame_animation</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>aa1c75de10ba908b20b3384fc09d857e4</anchor>
      <arglist>(int argc, char **argv, char *response, size_t response_size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>cmd_stop_animations</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>a77618cd8f49e406c505e2c0f8110efd2</anchor>
      <arglist>(int argc, char **argv, char *response, size_t response_size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>cmd_animation_status</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>a7aea8ea2734d3e76a9cb31e39f8ecaa7</anchor>
      <arglist>(int argc, char **argv, char *response, size_t response_size)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>uart_endgame_command_dispatcher</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>a2c1f034f2745aa000fda3e70da59ccea</anchor>
      <arglist>(int argc, char **argv)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>uart_stop_command_dispatcher</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>ae57225727889bb7bd8541d02660e33a6</anchor>
      <arglist>(int argc, char **argv)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>uart_animation_command_dispatcher</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>a6bd963956e3a304abd305b05f126b5fc</anchor>
      <arglist>(int argc, char **argv)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>handle_led_test_command</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>a25a982b28768b290cb56ecde22bf1560</anchor>
      <arglist>(char *argv[], int argc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>handle_led_pattern_command</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>a294aca3ab988c08ae29b5f13fe09d05a</anchor>
      <arglist>(char *argv[], int argc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>handle_led_animation_command</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>a0d0a671bd59866a24abecd0cd1ee9374</anchor>
      <arglist>(char *argv[], int argc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>handle_led_clear_command</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>a70c667d91da8b9165f7fbb815c32939b</anchor>
      <arglist>(char *argv[], int argc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>handle_led_brightness_command</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>a1eea20a3683af2f5c1a9782a8c12668c</anchor>
      <arglist>(char *argv[], int argc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>handle_chess_pos_command</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>aa86478fbc78b81c0beb5703735519706</anchor>
      <arglist>(char *argv[], int argc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>handle_led_mapping_test_command</name>
      <anchorfile>uart__commands__extended_8h.html</anchorfile>
      <anchor>ad8bb38053b28adcf807b63675070988c</anchor>
      <arglist>(char *argv[], int argc)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>uart_commands_extended.c</name>
    <path>components/uart_commands_extended/</path>
    <filename>uart__commands__extended_8c.html</filename>
    <includes id="uart__commands__extended_8h" name="uart_commands_extended.h" local="yes" import="no" module="no" objc="no">uart_commands_extended.h</includes>
    <includes id="game__led__animations_8h" name="game_led_animations.h" local="yes" import="no" module="no" objc="no">game_led_animations.h</includes>
    <includes id="led__mapping_8h" name="led_mapping.h" local="yes" import="no" module="no" objc="no">led_mapping.h</includes>
    <includes id="led__task_8h" name="led_task.h" local="yes" import="no" module="no" objc="no">led_task.h</includes>
    <member kind="function" static="yes">
      <type>static uint8_t</type>
      <name>parse_king_position</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>a077345de50407d4102d48247f457f88a</anchor>
      <arglist>(const char *pos_str)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>position_to_string</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>a2a78e4f624df8fcd55e57d48360a10e0</anchor>
      <arglist>(uint8_t pos, char *out_str)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>cmd_endgame_animations</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>ace118ff57c1728a13e4660df6234a4a8</anchor>
      <arglist>(int argc, char **argv, char *response, size_t response_size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>cmd_endgame_animation</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>aa1c75de10ba908b20b3384fc09d857e4</anchor>
      <arglist>(int argc, char **argv, char *response, size_t response_size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>cmd_stop_animations</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>a77618cd8f49e406c505e2c0f8110efd2</anchor>
      <arglist>(int argc, char **argv, char *response, size_t response_size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>cmd_animation_status</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>a7aea8ea2734d3e76a9cb31e39f8ecaa7</anchor>
      <arglist>(int argc, char **argv, char *response, size_t response_size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>register_extended_uart_commands</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>a91f740ce224db3050d1f85890fdbbdce</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>uart_endgame_command_dispatcher</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>a2c1f034f2745aa000fda3e70da59ccea</anchor>
      <arglist>(int argc, char **argv)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>uart_stop_command_dispatcher</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>ae57225727889bb7bd8541d02660e33a6</anchor>
      <arglist>(int argc, char **argv)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>uart_animation_command_dispatcher</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>a6bd963956e3a304abd305b05f126b5fc</anchor>
      <arglist>(int argc, char **argv)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>handle_led_test_command</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>a25a982b28768b290cb56ecde22bf1560</anchor>
      <arglist>(char *argv[], int argc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>handle_led_pattern_command</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>a294aca3ab988c08ae29b5f13fe09d05a</anchor>
      <arglist>(char *argv[], int argc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>handle_led_animation_command</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>a0d0a671bd59866a24abecd0cd1ee9374</anchor>
      <arglist>(char *argv[], int argc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>handle_led_clear_command</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>a70c667d91da8b9165f7fbb815c32939b</anchor>
      <arglist>(char *argv[], int argc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>handle_led_brightness_command</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>a1eea20a3683af2f5c1a9782a8c12668c</anchor>
      <arglist>(char *argv[], int argc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>handle_chess_pos_command</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>aa86478fbc78b81c0beb5703735519706</anchor>
      <arglist>(char *argv[], int argc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>handle_led_mapping_test_command</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>ad8bb38053b28adcf807b63675070988c</anchor>
      <arglist>(char *argv[], int argc)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>uart__commands__extended_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>uart_task.h</name>
    <path>components/uart_task/include/</path>
    <filename>uart__task_8h.html</filename>
    <includes id="chess__types_8h" name="chess_types.h" local="yes" import="no" module="no" objc="no">chess_types.h</includes>
    <class kind="struct">uart_message_t</class>
    <class kind="struct">uart_command_t</class>
    <member kind="typedef">
      <type>command_result_t(*</type>
      <name>command_handler_t</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a15db42d803f30c566b5f5fdf88d33b8c</anchor>
      <arglist>)(const char *args)</arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>command_result_t</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>ad67e3977ebd069018cc01fb52b551c63</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CMD_SUCCESS</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>ad67e3977ebd069018cc01fb52b551c63a0a22fcf0d061f9b1473876e43d12c33c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CMD_ERROR_INVALID_SYNTAX</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>ad67e3977ebd069018cc01fb52b551c63a9d8f4f8dab26ea6218e16579507f1a2c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CMD_ERROR_INVALID_PARAMETER</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>ad67e3977ebd069018cc01fb52b551c63a8e7eb0cf30a91dbf67498fef8dcba774</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CMD_ERROR_SYSTEM_ERROR</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>ad67e3977ebd069018cc01fb52b551c63aedd36bebd00d835aedb3b165d95687b1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CMD_ERROR_NOT_FOUND</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>ad67e3977ebd069018cc01fb52b551c63a8d68e7510dde3829311ed679636f0fd2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>uart_msg_type_t</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a708a8ff292163e24f246572bc7bf1036</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UART_MSG_NORMAL</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a708a8ff292163e24f246572bc7bf1036a70bc53ed1d6c2d1ea1f5b8f570ab166d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UART_MSG_ERROR</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a708a8ff292163e24f246572bc7bf1036abd403affd26add443eedf7089afadde4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UART_MSG_WARNING</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a708a8ff292163e24f246572bc7bf1036a85457d793a3de63c4eaa97f57bd36e18</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UART_MSG_SUCCESS</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a708a8ff292163e24f246572bc7bf1036a2483d473c9c1d73a35b38e1b35018158</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UART_MSG_INFO</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a708a8ff292163e24f246572bc7bf1036aeec4d640e5e10aa6c24208f817789452</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UART_MSG_DEBUG</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a708a8ff292163e24f246572bc7bf1036a877a61fdb522d990e286ad329d955f04</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_task_start</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a6e26b6d6ae554bd6bc478ccba6b1b763</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_write_string_immediate</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a417def5a37edadff4f93cf9e8824f082</anchor>
      <arglist>(const char *str)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_write_char_immediate</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a5d7ec4b42454dcce2b73a82b7e928484</anchor>
      <arglist>(char c)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_help</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a25d0a7a7a3f253ef4ba5f55e6764f879</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_verbose</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a771457bbb5dea20a3c8848f56ff30fb5</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_quiet</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a8bfc523766c1984380217c78bd865d93</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_status</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a21313e7215e44d82c987a6e19ec0dbf3</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_version</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a3d57139f7c4ab4244218b1c6151f48fc</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_memory</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>adeb40a444cec7562b4c9eae46fb5f905</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_history</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>ac75af37c48ea155d265417cfc6db8c95</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_clear</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>adbe2f7e4f05f1b0c256c81eefb34e95a</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_reset</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>af305f126d46460f209547def7ade998f</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_move</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a1fcb0efaeb8e64908c667d9a68a497e7</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_up</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>afa32b7335ecf868e1ad3f62ea110f1a7</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_dn</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>aa9bf90400c3b2ccf18c98442645b8b14</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_led_board</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a31e18c5d57fbf73693ba15d6041bd080</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_board</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>ae06c8535f9c7efcb9bf76c0949b0b4cc</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_game_new</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>aec33730164148a5ef9df45f1e1ae706b</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_game_reset</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a820002ffc256bb584099e401f1b07e9c</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_show_moves</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a221e0e7d3b8aaa2836b099cb13e57168</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_undo</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>ad5f1031b571a80d5b86af97d1f95c57c</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_game_history</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a45e7ff8cb362715634eba2642afae063</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_benchmark</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a5208e133190e19aad814c5a69dbb93e5</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_show_tasks</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>ad71e3397b99aa15ed45f6b5fe3f070d1</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_self_test</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>af34a40710b1a30f20f8be55971986884</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_test_game</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>ab237e12799096414a354e1023a69b98d</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_debug_status</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a925e6e86c81a91f4d6af38c6106833bc</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_debug_game</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>afed806feda00e8b2e6122fbba6a0800d</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_debug_board</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a8def6eb3ad5e05f605577170ea53085f</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_memcheck</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a8b0cefcf29f3dea4338fdd3475476f1a</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_show_mutexes</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a2e7e839a6c40f74905836d7d5d4fd69b</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_show_fifos</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a260841cc4caf0ecd44bc45719f3a6e6a</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_display_main_help</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a751f514352196d75fcb5afef4a54bc76</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_cmd_help_game</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a8173679d73f567977d5b63259363383c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_cmd_help_system</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a966ff272c2023dd69fee0aec745c949e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_cmd_help_beginner</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>ae225cfb5aecbfa679ca067a2e4dc6833</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_cmd_help_debug</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>aada1de37a1e46c05acb740016334d649</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_display_move_animation</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a0ef783ccf8a8c6f104f5ad95dc7c0faf</anchor>
      <arglist>(const char *from, const char *to)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_display_enhanced_board</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>ab33165e3f027da22b5695d323a262466</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_display_led_board</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>abc55b8a694daa72d150b869d5e4e0a7d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>is_valid_move_notation</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a19705a349c243f2fcaca1d209b11aa26</anchor>
      <arglist>(const char *move)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>is_valid_square_notation</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a27eda0257802e0a1d44cbe8b91e3feba</anchor>
      <arglist>(const char *square)</arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>uart_output_queue</name>
      <anchorfile>uart__task_8h.html</anchorfile>
      <anchor>a6e06eb5d048532e903f597094c601c4c</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>uart_task.c</name>
    <path>components/uart_task/</path>
    <filename>uart__task_8c.html</filename>
    <includes id="chess__types_8h" name="chess_types.h" local="yes" import="no" module="no" objc="no">../freertos_chess/include/chess_types.h</includes>
    <includes id="ha__light__task_8h" name="ha_light_task.h" local="yes" import="no" module="no" objc="no">../ha_light_task/include/ha_light_task.h</includes>
    <includes id="matrix__task_8h" name="matrix_task.h" local="yes" import="no" module="no" objc="no">../matrix_task/include/matrix_task.h</includes>
    <includes id="timer__system_8h" name="timer_system.h" local="yes" import="no" module="no" objc="no">../timer_system/include/timer_system.h</includes>
    <includes id="uart__commands__extended_8h" name="uart_commands_extended.h" local="yes" import="no" module="no" objc="no">../uart_commands_extended/include/uart_commands_extended.h</includes>
    <includes id="unified__animation__manager_8h" name="unified_animation_manager.h" local="yes" import="no" module="no" objc="no">../unified_animation_manager/include/unified_animation_manager.h</includes>
    <includes id="web__server__task_8h" name="web_server_task.h" local="yes" import="no" module="no" objc="no">../web_server_task/include/web_server_task.h</includes>
    <includes id="config__manager_8h" name="config_manager.h" local="yes" import="no" module="no" objc="no">config_manager.h</includes>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <includes id="game__task_8h" name="game_task.h" local="yes" import="no" module="no" objc="no">game_task.h</includes>
    <includes id="led__mapping_8h" name="led_mapping.h" local="yes" import="no" module="no" objc="no">led_mapping.h</includes>
    <includes id="led__task_8h" name="led_task.h" local="yes" import="no" module="no" objc="no">led_task.h</includes>
    <includes id="uart__task_8h" name="uart_task.h" local="yes" import="no" module="no" objc="no">uart_task.h</includes>
    <class kind="struct">input_buffer_t</class>
    <class kind="struct">command_history_t</class>
    <member kind="define">
      <type>#define</type>
      <name>SAFE_WDT_RESET</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a817d1b9d4253b32aa883d9bf92aae705</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHUNKED_PRINTF</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a48bf3d6d00a12d5a505d4e9a2241b2df</anchor>
      <arglist>(format,...)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHUNK_DELAY_MS</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aa992fb45f0b110c77fb61ee14f43012f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX_CHUNK_SIZE</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a523eaf5b650f38c307a6aab7f957fa2b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STACK_SAFETY_LIMIT</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>afaa4f2205de94aa018ae84d9968d3a09</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>UART_PORT_NUM</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ab74ec82c9f5211b716c017e6f4b26c4d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>UART_ENABLED</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>add7b6c4234d476bd8bb529019b9f48e9</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>UART_BAUD_RATE</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a615aed21aa6825462b7c17b0c238ffe2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>UART_BUF_SIZE</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ab0249d81dd53336b7810ebcf29511210</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>UART_CMD_BUFFER_SIZE</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a97e69bb8611929b4aeb9173738c7822e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>UART_CMD_HISTORY_SIZE</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aab0e9d8131e646f5ed7caff255394865</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>UART_MAX_ARGS</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a92730fc97f7b604391b5b3f913ad824e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>INPUT_TIMEOUT_MS</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aeac6e5795cef1c6971bde245d54de5d5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHAR_BACKSPACE</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a8517f7179ead44a22ff3b74051146ec5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHAR_DELETE</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a5f2ce9e9cba7370bacc763ede5150d5b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHAR_ENTER</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a1c1ac78fc6a82e6daa73b04926db7dd5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHAR_NEWLINE</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a44b71e8b06af27307dca69e34049f38f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHAR_ESC</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ac763d1de8ee96951064bd7dbb649dfc5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHAR_CTRL_C</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a5307e51b79c48d5bfc3853ea6df98a32</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHAR_CTRL_D</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a777f5bfc806d6ffe1d2b22b5615b9f46</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_CLEAR_LINE</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a98051331efc7fbe841adfaa6e359a26a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_CURSOR_LEFT</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ab4fc8f34cc7e3b4b32035605f6fcedf9</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_CURSOR_RIGHT</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a552e4dda434cca9e6e4c4ce6caa8765e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ANSI_CLEAR_TO_END</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a54bbc18dc1f760f32f69ac714dc6ff88</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_RESET</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a17f760256046df23dd0ab46602f04d02</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_GREEN</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>afc9149f5de51bd9ac4f5ebbfa153f018</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_RED</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ad86358bf19927183dd7b4ae215a29731</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_YELLOW</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a4534b577b74a58b0f4b7be027af664e0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_BLUE</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a23c70d699a5a775bc2e1ebeb8603f630</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_CYAN</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a82573859711fce56f1aa0a76b18a9b18</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_BOLD</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a302607d6f585b7e9ba1c3eab8d2199d5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_RESET</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a277f8b5333f79ffa0bdb99a8c330a128</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_RED</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a85d729dfbd398f62fba5def79d54027d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_GREEN</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a02d07b5153af9e6675e1c075be4c389a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_YELLOW</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ab126f96b0c5e120f2880f3cf1a4d912e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_BLUE</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a8000e32f660a001ae0eade53a5285f68</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MAGENTA</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a8deb0beccea721b35bdb1b4f7264fe75</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_CYAN</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a20f02c77b7d167ee80a1569801e12183</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_WHITE</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a9b44987ffdc2af19b635206b94334b69</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_BOLD</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a3dc61083272e4a370f281baf2bebeb61</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_DIM</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a82f3830461d1f2f3a8384936fb926920</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_ERROR</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ac7bab6591a09366d23b86d710ecc54af</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_SUCCESS</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a49ad0d2196700c1fa0a3a2a60dad41d3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_WARNING</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a4d79f3f59a4d19b93eaf106105bddc90</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_INFO</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a0d4d3a685f9501631d1feed1f4ba7577</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MOVE</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a22cb5d80da7468ab7c7060864119724a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_STATUS</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a42bae9393fad14add16d8fe270fb084f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_DEBUG</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a2799f0e4fea941044c782585431e22cb</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_HELP</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a412791c6c0d86fbfe8ca2c05b9b4b020</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>esc_state_t</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a945fae464be4b4e1c782a66a4401856f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ESC_STATE_NONE</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a945fae464be4b4e1c782a66a4401856fa08f8d80eed5895fa184c992527d86067</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ESC_STATE_ESC</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a945fae464be4b4e1c782a66a4401856facb373f962a04b4ce988e888e6cee6105</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ESC_STATE_BRACKET</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a945fae464be4b4e1c782a66a4401856fa59c33a4e0667c2c411c151ea99b8aab4</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>convert_notation_to_coords</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a20b79ef4675f25cd3f39ddcdaf65243c</anchor>
      <arglist>(const char *notation, uint8_t *row, uint8_t *col)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>uart_task_wdt_reset_safe</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ad5177f32635cd71c67857da386ce56a1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>uart_task_legacy_loop</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a85c2fc30eea2313aa471e3124760be42</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>uart_fputs</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a40999291ae23f6ab62823ec1a1595e73</anchor>
      <arglist>(const char *str)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_send_line</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a5e73cb389b8160054b07da47410b12da</anchor>
      <arglist>(const char *str)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>is_valid_move_notation</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a19705a349c243f2fcaca1d209b11aa26</anchor>
      <arglist>(const char *move)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>is_valid_square_notation</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a27eda0257802e0a1d44cbe8b91e3feba</anchor>
      <arglist>(const char *square)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_write_char_immediate</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a91fd98a1d105024cd8e69edeaed85870</anchor>
      <arglist>(char ch)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_write_string_immediate</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a417def5a37edadff4f93cf9e8824f082</anchor>
      <arglist>(const char *str)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_send_welcome_logo</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a1d46d56c2561b7b6cc93324464713296</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_show_progress_bar</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a10ae51d97bb118f2350314fa36455bd9</anchor>
      <arglist>(const char *label, uint32_t max_value, uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_send_colored</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a39a36d85fdb280e813cc1844f95af68d</anchor>
      <arglist>(const char *color, const char *message)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_send_colored_line</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a24fe1d895d08ee05b18db4ebe08f7aa4</anchor>
      <arglist>(const char *color, const char *message)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_send_error</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a997485823479f84fcbc9b5121128fb8b</anchor>
      <arglist>(const char *message)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_send_success</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aadc15ad30b458f1f25f63c2400ed3e4e</anchor>
      <arglist>(const char *message)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_send_warning</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>afeb7b780595651e1cd61ff63516ebf09</anchor>
      <arglist>(const char *message)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_send_info</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>af239b46472032f467e2a14bd6fb11109</anchor>
      <arglist>(const char *message)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_send_move</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a415daeebd03dc489051a6019ab57d86c</anchor>
      <arglist>(const char *message)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_send_status</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a014e7bd605b6ca9b3d4236077de47aee</anchor>
      <arglist>(const char *message)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_send_debug</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>abc487589c7d53fb643128b9d692fd095</anchor>
      <arglist>(const char *message)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_send_help</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aceeac1cd0533d37dd67318c4133e6845</anchor>
      <arglist>(const char *message)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_send_formatted</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a3fe9e611dc4f252d5c7cb8cc26ecb232</anchor>
      <arglist>(const char *format,...)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_send_string</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>adc5ab09aa19841e18a039de4b3250da4</anchor>
      <arglist>(const char *str)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_queue_message</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>af1ca9bbdbbd1329b9bade0cc31634ba7</anchor>
      <arglist>(uart_msg_type_t type, bool add_newline, const char *format,...)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>uart_process_output_queue</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a18b6871f5839ec673f8d865a6e90c56b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>input_buffer_init</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a555be973048afe2bebf55398cd905f63</anchor>
      <arglist>(input_buffer_t *buffer)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>input_buffer_clear</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a1cc51f2f4fea08ccfc228ca497cd34ae</anchor>
      <arglist>(input_buffer_t *buffer)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>input_buffer_add_char</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a8c1834a32f3797bf298c2940eb5ba7e6</anchor>
      <arglist>(input_buffer_t *buffer, char c)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>input_buffer_backspace</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a26c0e92edbcb05f59a70a602c07715dc</anchor>
      <arglist>(input_buffer_t *buffer)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>input_buffer_set_cursor</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a72e6311faaf01353872e8bc49484390c</anchor>
      <arglist>(input_buffer_t *buffer, size_t pos)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>command_history_init</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a0e4cb2a6e4b052fbab611501a057bd1d</anchor>
      <arglist>(command_history_t *history)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>command_history_add</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a7efaa3faedbd05c45b0319cbb8ba9a17</anchor>
      <arglist>(command_history_t *history, const char *command)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>command_history_get_previous</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>afb0a5da76f96fc4b5d90bba3f72b5253</anchor>
      <arglist>(command_history_t *history)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>command_history_get_next</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a66144e793ae8758d91e06b8559639464</anchor>
      <arglist>(command_history_t *history)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>command_history_show</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>af876cff135244df2d98dc26153dffeff</anchor>
      <arglist>(command_history_t *history)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>refresh_input_display</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ad1780c902a974c1328d45972b588d593</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>handle_arrow_up</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ad4eba791e9c70bcdc5084bda677d813e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>handle_arrow_down</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a77d973ce96a8003680dbbcdc89c58a93</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_eval</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a537655f93f38edf63de5af06fd0aeb09</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_ledtest</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ac0b5541aa713dea4c61168ad9bb49118</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_performance</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a2892ffc572084d804341dfb6817c3bdd</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_config</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a2514d071160fb4579a28e402990700c2</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_timer</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a475b0f6db66e3d1b412220cf21179f73</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_timer_config</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ae137c42e9c2b523702668600a4126f53</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_timer_pause</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a749364e3a9de6bde7c09d339c88a1440</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_timer_resume</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>af7a0ee931e10dbc1d27d1cbea7fee4ec</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_timer_reset</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aae8ad10d8170f6fc652c11ff484ff48a</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_wifi</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a5b73406393efc0efe0786fc87e571a22</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_wifi_connect</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>adbf3e3944f1845925bd08459c6b7cb89</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_wifi_disconnect</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a1eebbc47571699c8bb5c8da5ce40cd6a</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_wifi_status</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a011169e6aa5d6b3f141448e2864decbf</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_wifi_clear</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a703be1161dc49f25dd0e476c596443f9</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_web_lock</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a7f1538a5485ed6e27c22829f066d3a87</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_web_status</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aa9f2f59b642c88983b3335005fa13d04</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_mqtt_config</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a4cf583b305279172da15e67506989102</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_mqtt_status</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a8a67f06fc8ba00682d61c5c380ce612b</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_mqtt_test</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a0eaf4451977618b26df3ef6c207edfb1</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_demo</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ae3e1d270bce963f46ae25884ff307b33</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_demo_status</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a741b46c24c6a12f798156859a8dcf1a6</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_castle</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ad61d8dbd1ef7a5922ef37a348adf68d8</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_promote</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ae81c2f84dfde570afff3a8a6958fd1f5</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_matrixtest</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a4ba7a32bbded6ac3f4190f09c402cd84</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_component_off</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a5aba3dc71e5eb5272b490fe7990161d2</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_component_on</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a117b20f2d5162f335afe54a4175762c1</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_endgame_white</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a4e7fb6cbbcd52c91f5888a37233bc724</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_endgame_black</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a9ac40dded30f0a4876a7598759eec102</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_display_advantage_graph</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a631518fef77433c1a07e8226a1bcf0dc</anchor>
      <arglist>(uint32_t move_count, bool white_wins)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_test_move_anim</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a61e57d14da346b1c1274575582f37555</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_test_player_anim</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aa22b03cae44e7d502a86a2914142fb58</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_test_castle_anim</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a838bac457de362abcdb9e05926aaa3d6</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_test_promote_anim</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a01f571b03ab2582d70eba7d43025e204</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_test_endgame_anim</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aa5d62cda824f52c80201790ad65f6b70</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_endgame_wave</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a8dd59eef94833568e925c91f4ec2d49e</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_endgame_circles</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a031f5b4e479714bafc64d1ff08e8ad6c</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_endgame_cascade</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a9c4117773dc018eb25b4024ce6928bf4</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_endgame_fireworks</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a0614376ec50c99cf0d057f1d7f8eb15c</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_endgame_draw_spiral</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ab19ad9d65d0a70888ec3bb3e1bd98cc2</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_endgame_draw_pulse</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aa6bf2d9f00a64480e5888c5802d7b9b3</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_cmd_help_web</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a96698e984b9b078213389d0961ccb82c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_stop_endgame</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a29ce1ca9c76f9d6c2c36839d10be5d31</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_help</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a25d0a7a7a3f253ef4ba5f55e6764f879</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_display_main_help</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a751f514352196d75fcb5afef4a54bc76</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_cmd_help_game</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a8173679d73f567977d5b63259363383c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_cmd_help_system</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a966ff272c2023dd69fee0aec745c949e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_cmd_help_beginner</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ae225cfb5aecbfa679ca067a2e4dc6833</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_cmd_help_debug</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aada1de37a1e46c05acb740016334d649</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_verbose</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a771457bbb5dea20a3c8848f56ff30fb5</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_quiet</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a8bfc523766c1984380217c78bd865d93</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_status</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a21313e7215e44d82c987a6e19ec0dbf3</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_version</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a3d57139f7c4ab4244218b1c6151f48fc</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_memory</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>adeb40a444cec7562b4c9eae46fb5f905</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_history</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ac75af37c48ea155d265417cfc6db8c95</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_clear</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>adbe2f7e4f05f1b0c256c81eefb34e95a</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_reset</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>af305f126d46460f209547def7ade998f</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>format_time_mmss</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>abc07526e4d22deb256e3182b5f6857b8</anchor>
      <arglist>(char *buffer, size_t size, uint32_t time_ms)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>parse_move_notation</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a5375af563f4375b03e16cb5516f3e06f</anchor>
      <arglist>(const char *input, char *from, char *to)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>validate_chess_squares</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a86e9ed02bc7633c27662831ea262f799</anchor>
      <arglist>(const char *from, const char *to)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>send_to_game_task</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a557b6aeba60814f680be2e1cc9dd8164</anchor>
      <arglist>(const chess_move_command_t *move_cmd)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>send_to_game_task_with_response</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a3e794219ce8f13265cbb99ff00780a1b</anchor>
      <arglist>(const chess_move_command_t *move_cmd, char *response_buffer, size_t buffer_size, uint32_t timeout_ms)</arglist>
    </member>
    <member kind="function">
      <type>const uart_command_t *</type>
      <name>find_command</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a35d6750b6e64838fc7397da5f65c0056</anchor>
      <arglist>(const char *command)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>execute_command</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a015b3be04ff0b73780f9c39e0197a98f</anchor>
      <arglist>(const char *command, const char *args)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_parse_command</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>abd86b6f2266678ad987dbac5f16f055d</anchor>
      <arglist>(const char *input)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>uart_check_memory_health</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a7221e5806cd35fd8640c4f3ef239f9cc</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_task_recover_from_error</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a2ac3869f9cbee04268f1aa147aa4eee7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>uart_task_health_check</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a80e3cf486dec37b0b9430b78bcb253be</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_process_input</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a9a071144fffaabac5718c2b216dfcf21</anchor>
      <arglist>(char c)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>get_unicode_piece_symbol</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a343dbfc6b59f2de2fc9d42645d52dda9</anchor>
      <arglist>(piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>get_ascii_piece_symbol</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a23cdba6f3e9cde28600d5540afcc0531</anchor>
      <arglist>(piece_t piece)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_move</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a1fcb0efaeb8e64908c667d9a68a497e7</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_display_move_animation</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a0ef783ccf8a8c6f104f5ad95dc7c0faf</anchor>
      <arglist>(const char *from, const char *to)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_up</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>afa32b7335ecf868e1ad3f62ea110f1a7</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_dn</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aa9bf90400c3b2ccf18c98442645b8b14</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_board</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ae06c8535f9c7efcb9bf76c0949b0b4cc</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_led_board</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a31e18c5d57fbf73693ba15d6041bd080</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_display_enhanced_board</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ab33165e3f027da22b5695d323a262466</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_game_new</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aec33730164148a5ef9df45f1e1ae706b</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_game_reset</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a820002ffc256bb584099e401f1b07e9c</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_show_moves</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a221e0e7d3b8aaa2836b099cb13e57168</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_undo</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ad5f1031b571a80d5b86af97d1f95c57c</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_game_history</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a45e7ff8cb362715634eba2642afae063</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_self_test</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>af34a40710b1a30f20f8be55971986884</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_test_game</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ab237e12799096414a354e1023a69b98d</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_debug_status</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a925e6e86c81a91f4d6af38c6106833bc</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_debug_game</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>afed806feda00e8b2e6122fbba6a0800d</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_debug_board</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a8def6eb3ad5e05f605577170ea53085f</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_benchmark</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a5208e133190e19aad814c5a69dbb93e5</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_memcheck</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a8b0cefcf29f3dea4338fdd3475476f1a</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_show_tasks</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ad71e3397b99aa15ed45f6b5fe3f070d1</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_show_mutexes</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a2e7e839a6c40f74905836d7d5d4fd69b</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>display_queue_status</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a423d6521216e84e883b8ef6b3c8d535b</anchor>
      <arglist>(const char *name, QueueHandle_t queue)</arglist>
    </member>
    <member kind="function">
      <type>command_result_t</type>
      <name>uart_cmd_show_fifos</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a260841cc4caf0ecd44bc45719f3a6e6a</anchor>
      <arglist>(const char *args)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_task_start</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a6e26b6d6ae554bd6bc478ccba6b1b763</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>uart_display_led_board</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>abc55b8a694daa72d150b869d5e4e0a7d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable">
      <type>SemaphoreHandle_t</type>
      <name>uart_mutex</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a8eca84d3e2c7c0adf364819e74b1bd9b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>led_task_handle</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a6cf81c896b094e20ff5752ee1133a8f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>matrix_task_handle</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a9de1b87e0b6692c90f1c5cfd9e13d444</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>button_task_handle</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aba5c2db71f9fa5cd9f3e506e91ffe6d3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>game_task_handle</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>acb49a688d90460dc48f7e0b976ace62d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>game_command_queue</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a4ed770cbe46604586e494bae84b62304</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>matrix_component_enabled</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>add7c22b0e6811989af669309caa12fe8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>led_component_enabled</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a8bfc7db202490dafea98d0ede64bc881</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>wifi_component_enabled</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a6196f7e1db4cb31f634fb82f481f1965</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>task_running</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a35141cb45623cb3b7b4d96da35ed1831</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>color_enabled</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ab6377eecee38b3e695c2d63d181d3ed2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static input_buffer_t</type>
      <name>input_buffer</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ad70cd585173babaab9affd32c7f8abee</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static command_history_t</type>
      <name>command_history</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a61191483532939b033d38260bf51a471</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static system_config_t</type>
      <name>system_config</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a6f2695a4ef01bcb7a046b88a8688c6ae</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static esc_state_t</type>
      <name>esc_state</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>ad769b94f8ae1483e3b5018b93c19fe3b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static int</type>
      <name>history_navigation_index</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>af64b20d5ad64044c58134e0b9a19d398</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>uart_output_queue</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a6e06eb5d048532e903f597094c601c4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>command_count</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a45227fd9b16b7d96747cb21a06822165</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>error_count</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>aedd33235b875180b4a57e5bd210cadd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>last_command_time</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a8c4e957de70c57192735974adca4c520</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const uart_command_t</type>
      <name>commands</name>
      <anchorfile>uart__task_8c.html</anchorfile>
      <anchor>a537f725e34a30bac0d5b26630bf7ecda</anchor>
      <arglist>[]</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>unified_animation_manager.h</name>
    <path>components/unified_animation_manager/include/</path>
    <filename>unified__animation__manager_8h.html</filename>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <class kind="struct">animation_config_t</class>
    <class kind="struct">animation_state_struct</class>
    <member kind="typedef">
      <type>struct animation_state_struct</type>
      <name>animation_state_t</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>ad6143ee8f8a7f6e8a58b51a947a36fa9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>animation_priority_t</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a1d14f93d2747753699b61ea7f84c97a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_PRIORITY_BACKGROUND</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a1d14f93d2747753699b61ea7f84c97a6ac6ce09baa3112b33b63cef95c89f2a4b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_PRIORITY_LOW</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a1d14f93d2747753699b61ea7f84c97a6aaf6efdcd03fbd8fe3574d90a8dd8d83f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_PRIORITY_MEDIUM</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a1d14f93d2747753699b61ea7f84c97a6a0124adadcd7f68b27f3609f4cf01c0e4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_PRIORITY_HIGH</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a1d14f93d2747753699b61ea7f84c97a6ab7d8cfeacc7585107aebcc81bf3e0d13</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_PRIORITY_CRITICAL</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a1d14f93d2747753699b61ea7f84c97a6a2a7c5c9cb77a6948c0e4a829ca21a032</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_PRIORITY_AMBIENT</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a1d14f93d2747753699b61ea7f84c97a6a3fd1a5a62f63ec47085a395af6f1ba2d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_PRIORITY_GAME</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a1d14f93d2747753699b61ea7f84c97a6ac05057ec5b13965afd7265b2794b1f1c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_PRIORITY_INTERACTION</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a1d14f93d2747753699b61ea7f84c97a6a01f20e5298d5ca321dc3904b63562e7b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_PRIORITY_ALERT</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a1d14f93d2747753699b61ea7f84c97a6acba2ed376381afd69a926d2344151d42</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>animation_type_t</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690df</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_MOVE_PATH</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfaba93e256ac856258b46046b4e5c88d04</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_PIECE_GUIDANCE</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa8c45a77c12dfce36851aa4c537e7e423</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_VALID_MOVES</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfacf2ef3fb5f8340d5c501fd9a475df83b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_ERROR_FLASH</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa929893df03abbb9a2f60e74ddabefbe2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_CAPTURE_EFFECT</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa08010fb2f9d9b7c15f01e516ebc7b82a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_CHECK_WARNING</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa8b41d1d933100a647d965fffc1d8e3f3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_GAME_END</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa9db26c645dc66b254f7dca8ba09f3186</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_PLAYER_CHANGE</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa7b7026e9f90fd17797fd049300503be2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_CASTLE</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa309bada68e8942a4bf6723c4a2d8e2bd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_PROMOTION</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa920c21061e89bd671f1978728476f680</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_CONFIRMATION</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa35c98ba1500c10c7bb022674206cd0b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_ENDGAME_WAVE</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa9c55fe3aa6ceab2f9b0854cf4c57ebae</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_ENDGAME_CIRCLES</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa9e37be820b57920ebf21df831a3aa1d1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_ENDGAME_CASCADE</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa3a438bd15e4237a413b3a45dfbee4675</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_ENDGAME_FIREWORKS</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa640e0be580822351e16e4cd68c4351ea</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_ENDGAME_DRAW_SPIRAL</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfaec0321e780fd3731c5cbb52e10a52901</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_ENDGAME_DRAW_PULSE</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa01a8550d7489dc34d42bf1668684ad8c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ANIM_TYPE_COUNT</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e703c09ba390bb0306ced9d307690dfa6231b5cedda22e116742dfd0a51ec7d4</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_manager_init</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>abe2de9761634e293a13f1f493274aa8e</anchor>
      <arglist>(const animation_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_manager_deinit</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>ae6ff7c3686a9e96ec165a58721319443</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>unified_animation_create</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a94483a3f8c95cb093d2a278b8cde1c9e</anchor>
      <arglist>(animation_type_t type, animation_priority_t priority)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>unified_animation_stop</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a11b526f2f45df5e367c9ec63500c7fc1</anchor>
      <arglist>(uint32_t anim_id)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>unified_animation_stop_all</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a00e3ed0913caba4f42ed8749b9e41d6f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>animation_start</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a18ec392f54e4f1b713dcb9aae5122336</anchor>
      <arglist>(animation_type_t type, uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col, uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>animation_start_simple</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a44d3ff5fe3b1258813e6552946435646</anchor>
      <arglist>(animation_type_t type, uint8_t row, uint8_t col, uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_stop</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a10c9d7146d06f86543ad17f26e2a9d95</anchor>
      <arglist>(uint32_t animation_id)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_stop_all_of_type</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>aa1f890d113566a136d9c2eff8fc41ea0</anchor>
      <arglist>(animation_type_t type)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_stop_all_up_to_priority</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a0171410732cf87be81e06aa55a558e98</anchor>
      <arglist>(animation_priority_t max_priority)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_stop_all</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a01927a2699e720c9b08560ef9399325d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_set_params</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a7e575a7208129e31d4ff7174b837d6e9</anchor>
      <arglist>(uint32_t animation_id, uint8_t speed, uint8_t intensity, bool looping)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_set_colors</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a6ddc9223b7f71b9f8f5036ce94232e27</anchor>
      <arglist>(uint32_t animation_id, uint8_t r_primary, uint8_t g_primary, uint8_t b_primary, uint8_t r_secondary, uint8_t g_secondary, uint8_t b_secondary)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_fade_out</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>ac92b5183b45c3294dc7adf36419e01cb</anchor>
      <arglist>(uint32_t animation_id, uint32_t fade_duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_set_completion_callback</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a05ddc83cc56c70288d5dfca85af00da6</anchor>
      <arglist>(uint32_t animation_id, void(*callback)(uint32_t))</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>animation_is_active</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>acaa0b94777258411e3878a2e39e54ff4</anchor>
      <arglist>(uint32_t animation_id)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>animation_get_active_count</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a449fc7b4f2f2b3cf900240d8cb32fbab</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>animation_get_count_by_priority</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a6f862f227ec3d53bbb04794ab2266791</anchor>
      <arglist>(animation_priority_t priority)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_print_status</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a9731cdb2cdcf0234b35af6ba0b7b1402</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_update_all</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a12fe6b712faed508d33146cdc1545473</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_manager_update</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a9bc50ba514bea1a1279e8ecb31a73244</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_move</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a694f6eb2c55639e1db5ae412d4d7a7ef</anchor>
      <arglist>(uint32_t anim_id, uint8_t from_led, uint8_t to_led, uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_guidance</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a865d7b302c8f41c03db42d4582ee053f</anchor>
      <arglist>(uint32_t anim_id, uint8_t *led_array, uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_error</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a881cf8eb8ba5108cffb4feb7ef24ddd0</anchor>
      <arglist>(uint32_t anim_id, uint8_t led_index, uint32_t flash_count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_promotion</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a6b72a254b0978ba666bddc35551e3a8d</anchor>
      <arglist>(uint32_t anim_id, uint8_t promotion_led)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_endgame_wave</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>aff6580f58fa39d825446daecc92ff0b2</anchor>
      <arglist>(uint32_t anim_id, uint8_t center_led, uint8_t winner_color)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_endgame_circles</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>aa71a3707a1b82c7f11477d902dc5ba7e</anchor>
      <arglist>(uint32_t anim_id, uint8_t center_led, uint8_t winner_color)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_endgame_cascade</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>ac8d933a0a6f26e8a53d88f5f0839386e</anchor>
      <arglist>(uint32_t anim_id, uint8_t center_led, uint8_t winner_color)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_endgame_fireworks</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>afe5c5efdb84c2b6f51b9b704cb9e83a8</anchor>
      <arglist>(uint32_t anim_id, uint8_t center_led, uint8_t winner_color)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_endgame_draw_spiral</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a36e475c8eb3e48ea57c29780f6058e78</anchor>
      <arglist>(uint32_t anim_id, uint8_t center_led)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_endgame_draw_pulse</name>
      <anchorfile>unified__animation__manager_8h.html</anchorfile>
      <anchor>a5e3e1269aa710141f350d3a185db6f08</anchor>
      <arglist>(uint32_t anim_id, uint8_t center_led)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>unified_animation_manager.c</name>
    <path>components/unified_animation_manager/</path>
    <filename>unified__animation__manager_8c.html</filename>
    <includes id="unified__animation__manager_8h" name="unified_animation_manager.h" local="yes" import="no" module="no" objc="no">unified_animation_manager.h</includes>
    <includes id="led__task_8h" name="led_task.h" local="yes" import="no" module="no" objc="no">led_task.h</includes>
    <includes id="led__mapping_8h" name="led_mapping.h" local="yes" import="no" module="no" objc="no">led_mapping.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>min</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>ac6afabdc09a49a433ee19d8a9486056d</anchor>
      <arglist>(a, b)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>animation_update_smooth_interpolation</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>ae5eb26e747498c71310a20a4192bc72f</anchor>
      <arglist>(animation_state_t *anim)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>animation_update_pulsing</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>ac04fe1937e5690c98392d978cc881e11</anchor>
      <arglist>(animation_state_t *anim)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>animation_update_flashing</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>af169c31b4728fd389cef9a2551f8947f</anchor>
      <arglist>(animation_state_t *anim)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>animation_update_rainbow</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a0b5d021f7052366508f19b0db7306d40</anchor>
      <arglist>(animation_state_t *anim)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>animation_update_endgame_wave</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>aafd47a4efcf1e963e6482eff96b5245f</anchor>
      <arglist>(animation_state_t *anim)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>animation_update_endgame_circles</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>abbf20a753fe8fa0727e662f11ee90345</anchor>
      <arglist>(animation_state_t *anim)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>animation_update_endgame_cascade</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a46689763d76e1db929b64d98231853ac</anchor>
      <arglist>(animation_state_t *anim)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>animation_update_endgame_fireworks</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>ad17f4fad4ae0f10e95213a0ce3133992</anchor>
      <arglist>(animation_state_t *anim)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>animation_update_endgame_draw_spiral</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>aa5f953a8aa6e4bffdd2f54b5cbc72287</anchor>
      <arglist>(animation_state_t *anim)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>animation_update_endgame_draw_pulse</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a92561caf8e46454153425870bdb25f86</anchor>
      <arglist>(animation_state_t *anim)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>animation_update_promotion</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a5921d60e99902cb914cb21751e6a6b1d</anchor>
      <arglist>(animation_state_t *anim)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>animation_is_valid_id</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a4f09ab7845e085532967e5bda04847ed</anchor>
      <arglist>(uint32_t anim_id)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static animation_state_t *</type>
      <name>animation_find_by_id</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a0cd70e6c0a667a6075d7d11ceec61451</anchor>
      <arglist>(uint32_t anim_id)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>animation_cleanup</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a164aec003efd8bc175f2ffa31694fa57</anchor>
      <arglist>(animation_state_t *anim)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static float</type>
      <name>ease_in_out_cubic</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a44a07045bc7d0b9019e122b6ee2f1c59</anchor>
      <arglist>(float t)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>interpolate_color</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a0e552d083d40a494e5bec582a329faac</anchor>
      <arglist>(uint8_t from_r, uint8_t from_g, uint8_t from_b, uint8_t to_r, uint8_t to_g, uint8_t to_b, float progress, uint8_t *out_r, uint8_t *out_g, uint8_t *out_b)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_manager_init</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>abe2de9761634e293a13f1f493274aa8e</anchor>
      <arglist>(const animation_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_manager_deinit</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>ae6ff7c3686a9e96ec165a58721319443</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>unified_animation_create</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a94483a3f8c95cb093d2a278b8cde1c9e</anchor>
      <arglist>(animation_type_t type, animation_priority_t priority)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_move</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a694f6eb2c55639e1db5ae412d4d7a7ef</anchor>
      <arglist>(uint32_t anim_id, uint8_t from_led, uint8_t to_led, uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_guidance</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a865d7b302c8f41c03db42d4582ee053f</anchor>
      <arglist>(uint32_t anim_id, uint8_t *led_array, uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_error</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a881cf8eb8ba5108cffb4feb7ef24ddd0</anchor>
      <arglist>(uint32_t anim_id, uint8_t led_index, uint32_t flash_count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_capture</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>aeb5fac10b7dd203cb0a5d078f6822bda</anchor>
      <arglist>(uint32_t anim_id, uint8_t led_index, uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_confirmation</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a76bbc5ad4b103bd31bdc2bd601684504</anchor>
      <arglist>(uint32_t anim_id, uint8_t led_index)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_endgame_draw_spiral</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a36e475c8eb3e48ea57c29780f6058e78</anchor>
      <arglist>(uint32_t anim_id, uint8_t center_led)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_endgame_draw_pulse</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a5e3e1269aa710141f350d3a185db6f08</anchor>
      <arglist>(uint32_t anim_id, uint8_t center_led)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_endgame_wave</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>aff6580f58fa39d825446daecc92ff0b2</anchor>
      <arglist>(uint32_t anim_id, uint8_t center_led, uint8_t winner_color)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_endgame_circles</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>aa71a3707a1b82c7f11477d902dc5ba7e</anchor>
      <arglist>(uint32_t anim_id, uint8_t center_led, uint8_t winner_color)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_endgame_cascade</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>ac8d933a0a6f26e8a53d88f5f0839386e</anchor>
      <arglist>(uint32_t anim_id, uint8_t center_led, uint8_t winner_color)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_endgame_fireworks</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>afe5c5efdb84c2b6f51b9b704cb9e83a8</anchor>
      <arglist>(uint32_t anim_id, uint8_t center_led, uint8_t winner_color)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>unified_animation_stop</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a11b526f2f45df5e367c9ec63500c7fc1</anchor>
      <arglist>(uint32_t anim_id)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_stop_all_except_priority</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>ae02f54c8a3a0d8afb0aa9464197b3783</anchor>
      <arglist>(animation_priority_t min_priority)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>unified_animation_stop_all</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a00e3ed0913caba4f42ed8749b9e41d6f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>animation_manager_update</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a9bc50ba514bea1a1279e8ecb31a73244</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>animation_is_running</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a86f6ae8f90264777401b852a9202bf3a</anchor>
      <arglist>(uint32_t anim_id)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>animation_get_active_count</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a449fc7b4f2f2b3cf900240d8cb32fbab</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>animation_get_active_count_by_priority</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>ad150f160a7f6d0eb9b5ef771babb4dd7</anchor>
      <arglist>(animation_priority_t priority)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_set_config</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>aa371ad1236f7b8be7ae05594a60c5cc5</anchor>
      <arglist>(const animation_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_set_default_duration</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>ab95793a00db8ed7da51122351a09118c</anchor>
      <arglist>(uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_set_update_frequency</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a3a668868f9c356dd7a43849fc9ea9783</anchor>
      <arglist>(uint8_t frequency_hz)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>animation_get_type_name</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>ad8fa6c8e6d94097dba44ab216e49019a</anchor>
      <arglist>(animation_type_t type)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>animation_get_priority_name</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>ad2efb6e514f013ee5204a67b5f63d88d</anchor>
      <arglist>(animation_priority_t priority)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_get_status</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>ad08789a012eae8685138f3c02f20f96a</anchor>
      <arglist>(char *buffer, size_t buffer_size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>animation_start_promotion</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a6b72a254b0978ba666bddc35551e3a8d</anchor>
      <arglist>(uint32_t anim_id, uint8_t promotion_led)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>manager_initialized</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a056b36df10f165cd5a6b993fc63fa152</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static animation_config_t</type>
      <name>current_config</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a2e2bf556f54a319a5416488e0290e4d4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static animation_state_t</type>
      <name>animations</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a4136accd8b976228e8391ba4c8e04879</anchor>
      <arglist>[16]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>next_animation_id</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>ad3b6e14b9ad4bca4c07f67b318ed0e79</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>last_update_time</name>
      <anchorfile>unified__animation__manager_8c.html</anchorfile>
      <anchor>a0bb6dcf519e5bcc59b766b1a23c2535c</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>visual_error_system.h</name>
    <path>components/visual_error_system/include/</path>
    <filename>visual__error__system_8h.html</filename>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <class kind="struct">error_guidance_t</class>
    <class kind="struct">error_system_config_t</class>
    <class kind="struct">active_error_t</class>
    <member kind="enumeration">
      <type></type>
      <name>visual_error_type_t</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a1cf2e6ca0a08364ccdf0656c212380da</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ERROR_VISUAL_INVALID_MOVE</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a1cf2e6ca0a08364ccdf0656c212380daaf8203fd08d0fbab4c43496ac51ca7ff6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ERROR_VISUAL_WRONG_TURN</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a1cf2e6ca0a08364ccdf0656c212380daa65797988f25b00cbddea2cbce6b38e46</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ERROR_VISUAL_NO_PIECE</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a1cf2e6ca0a08364ccdf0656c212380daa08d581ce669e4bc1cbd55c29e43032a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ERROR_VISUAL_PIECE_BLOCKING</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a1cf2e6ca0a08364ccdf0656c212380daaed9dfe2a8f8f05f26129f9da5350ceb0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ERROR_VISUAL_CHECK_VIOLATION</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a1cf2e6ca0a08364ccdf0656c212380daa50b4433db1a1a2fddea20bb8b7493f16</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ERROR_VISUAL_SYSTEM_ERROR</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a1cf2e6ca0a08364ccdf0656c212380daa39a6b8b12fd8e563fa0456178fddda19</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ERROR_VISUAL_INVALID_SYNTAX</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a1cf2e6ca0a08364ccdf0656c212380daa8296825b7936ac8bbe6081a3d050a8c7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ERROR_VISUAL_COUNT</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a1cf2e6ca0a08364ccdf0656c212380daaa4b2a9220810b2bab7cfee40145ece36</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>visual_error_init</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>acf7327bbf341147566353b938cb8c3b1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_system_init</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>ac995527f0c28a1e3c3e6182139d06430</anchor>
      <arglist>(const error_system_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_system_deinit</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>ad70233d9512da8d6108776c773e942e3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>visual_error_show</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a97a9c02412523701a0f1d5a37e845629</anchor>
      <arglist>(visual_error_type_t error_type, uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>visual_error_clear</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a314097cc7e5312622bbd6ad0981a7ece</anchor>
      <arglist>(uint32_t error_id)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>visual_error_clear_all</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a2a68c37b08f7acbbefa6b31d6a79c9d3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_clear_all_indicators</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>ac94fcd47b6a08516a34bd10162b320bd</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>visual_error_confirm</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>abdbfe23e7e752a51b021ff354fd6efe0</anchor>
      <arglist>(uint32_t error_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>visual_error_show_guidance</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>ab0e65f1cb6bc34b175f7e86e8a2b05fa</anchor>
      <arglist>(visual_error_type_t error_type)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>visual_error_print_details</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>abdd724f673b0a1e0a44c562e25861fcf</anchor>
      <arglist>(visual_error_type_t error_type)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>visual_error_highlight_positions</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a30f70b7cd5f45b60beccf0fe96481615</anchor>
      <arglist>(visual_error_type_t error_type, const uint8_t *positions, uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>visual_error_illegal_move</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a7773aff356d74233bfd8e6cc108934be</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>visual_error_wrong_turn</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a6060ef6aeb2a280f76fceabc6d78f68a</anchor>
      <arglist>(uint8_t row, uint8_t col)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>visual_error_piece_blocked</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>ac3a31019dad37c8b06f3cf349a409048</anchor>
      <arglist>(uint8_t row, uint8_t col, const uint8_t *blocked_positions, uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>visual_error_check_not_resolved</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a0f02317b17f5b0281f661f19deae1499</anchor>
      <arglist>(uint8_t king_row, uint8_t king_col, const uint8_t *threat_positions, uint8_t threat_count)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>visual_error_is_active</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a635ac5cf9d0bb5b24584fea8efe16bad</anchor>
      <arglist>(uint32_t error_id)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>visual_error_get_active_count</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a0f75ba7a0d65c983dfb87b2ed2ba0c68</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>visual_error_print_status</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a5dbb39b564ea07ad3489bba1f515b375</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_set_config</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>affeb147a450f8cb876aa449cd2810459</anchor>
      <arglist>(const error_system_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_set_flash_count</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>ae69d7d8836ef3c8815256f5522ca5dff</anchor>
      <arglist>(uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_set_flash_duration</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a0031642405bf7dfbf920a89dba3c10a4</anchor>
      <arglist>(uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_get_led_positions_for_move</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a05cea41b9d878ee84c04cfa3c4b8d987</anchor>
      <arglist>(const chess_move_t *move, uint8_t *led_positions, uint8_t *count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_get_led_positions_for_square</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a272ee1742d41b587ae4d72681377bb19</anchor>
      <arglist>(uint8_t row, uint8_t col, uint8_t *led_positions, uint8_t *count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_get_status</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a463e7fa72dbf8c609d0e2740791d969f</anchor>
      <arglist>(char *buffer, size_t buffer_size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_show_visual</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>aeb7e8d4b5498f2ea0e6fe7e48bf5cad5</anchor>
      <arglist>(visual_error_type_t error_type, const chess_move_t *failed_move)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_show_blocking_path</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a46114d68c2b1fcc68370988272a94445</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_show_check_escape_options</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a3cecc0d8e7cadda0a383b72eae511e69</anchor>
      <arglist>(uint8_t king_row, uint8_t king_col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_guide_correct_move</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a4c6190405f3084c622bcf74002ffabf4</anchor>
      <arglist>(const chess_move_t *correct_move)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_guide_piece_selection</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>ad9afe9c1a8c014529460b0fea1f05fa9</anchor>
      <arglist>(uint8_t *movable_pieces, uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_guide_valid_destinations</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a36edba382e6f3074f4305522f4b6df05</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>error_get_last_message</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a69eece35bee75f5f08536a8de6b06bdc</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>error_get_recovery_hint</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a4791aa096cc4979c684bea5221dc8f4f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_convert_move_error_to_visual</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>a4dda5bf9ea83f0ad9fc924e59f3b39f2</anchor>
      <arglist>(move_error_t move_error, visual_error_type_t *visual_type)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>visual_error_update</name>
      <anchorfile>visual__error__system_8h.html</anchorfile>
      <anchor>aaef03c095e26b53373ea8d44a3fe3af1</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>visual_error_system.c</name>
    <path>components/visual_error_system/</path>
    <filename>visual__error__system_8c.html</filename>
    <includes id="visual__error__system_8h" name="visual_error_system.h" local="yes" import="no" module="no" objc="no">visual_error_system.h</includes>
    <includes id="led__state__manager_8h" name="led_state_manager.h" local="yes" import="no" module="no" objc="no">led_state_manager.h</includes>
    <includes id="unified__animation__manager_8h" name="unified_animation_manager.h" local="yes" import="no" module="no" objc="no">unified_animation_manager.h</includes>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>error_flash_leds</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a54c1656bcbedc160755be684d142eb0d</anchor>
      <arglist>(uint8_t *led_positions, uint8_t count, uint8_t r, uint8_t g, uint8_t b)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>error_pulse_leds</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>aeba836e63e2862dbe66eaf5bb661b3e2</anchor>
      <arglist>(uint8_t *led_positions, uint8_t count, uint8_t r, uint8_t g, uint8_t b)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const char *</type>
      <name>error_get_message_for_type</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a22a3df3379553a74340a2c29dc7daa8e</anchor>
      <arglist>(visual_error_type_t type)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const char *</type>
      <name>error_get_hint_for_type</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a5c587418bdd52f02272c53ec8568d77b</anchor>
      <arglist>(visual_error_type_t type)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>error_get_color_for_type</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>aabbf0dffdf236db70fe15daa48dcd3ec</anchor>
      <arglist>(visual_error_type_t type, uint8_t *r, uint8_t *g, uint8_t *b)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_system_init</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>ac995527f0c28a1e3c3e6182139d06430</anchor>
      <arglist>(const error_system_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_system_deinit</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>ad70233d9512da8d6108776c773e942e3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_show_visual</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>aeb7e8d4b5498f2ea0e6fe7e48bf5cad5</anchor>
      <arglist>(visual_error_type_t error_type, const chess_move_t *failed_move)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_show_blocking_path</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a46114d68c2b1fcc68370988272a94445</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_show_check_escape_options</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a3cecc0d8e7cadda0a383b72eae511e69</anchor>
      <arglist>(uint8_t king_row, uint8_t king_col)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_clear_all_indicators</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>ac94fcd47b6a08516a34bd10162b320bd</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_guide_correct_move</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a7f8dc092b20a9a0638066774da141476</anchor>
      <arglist>(const chess_move_t *suggested_move)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_guide_piece_selection</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>ad9afe9c1a8c014529460b0fea1f05fa9</anchor>
      <arglist>(uint8_t *movable_pieces, uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_guide_valid_destinations</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a36edba382e6f3074f4305522f4b6df05</anchor>
      <arglist>(uint8_t from_row, uint8_t from_col)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>error_get_last_message</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a69eece35bee75f5f08536a8de6b06bdc</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>error_get_recovery_hint</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a4791aa096cc4979c684bea5221dc8f4f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>error_get_error_type_name</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a43209f153740f14d9ad4fcbbf9cb94ce</anchor>
      <arglist>(visual_error_type_t type)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_get_status</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a463e7fa72dbf8c609d0e2740791d969f</anchor>
      <arglist>(char *buffer, size_t buffer_size)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>error_is_displaying</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a4470983fe742a87855a011f765d9137c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint8_t</type>
      <name>error_get_active_count</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a4e04de1e171c71b4c32364f0ae8b232e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_set_config</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>affeb147a450f8cb876aa449cd2810459</anchor>
      <arglist>(const error_system_config_t *config)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_set_flash_count</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>ae69d7d8836ef3c8815256f5522ca5dff</anchor>
      <arglist>(uint8_t count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_set_flash_duration</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a0031642405bf7dfbf920a89dba3c10a4</anchor>
      <arglist>(uint32_t duration_ms)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_convert_move_error_to_visual</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a4dda5bf9ea83f0ad9fc924e59f3b39f2</anchor>
      <arglist>(move_error_t move_error, visual_error_type_t *visual_type)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_get_led_positions_for_move</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a05cea41b9d878ee84c04cfa3c4b8d987</anchor>
      <arglist>(const chess_move_t *move, uint8_t *led_positions, uint8_t *count)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>error_get_led_positions_for_square</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a272ee1742d41b587ae4d72681377bb19</anchor>
      <arglist>(uint8_t row, uint8_t col, uint8_t *led_positions, uint8_t *count)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>system_initialized</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>ab2336e816f0a31f0e3e85fba396e4340</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static error_system_config_t</type>
      <name>current_config</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>af09b5a75ac80e161207fc294e3e25b0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static error_guidance_t</type>
      <name>last_error</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a6f820630feb824600d909cbb27937851</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>displaying_error</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>ab48772c5b7b95dc1acb27d1e15a62b33</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>active_error_count</name>
      <anchorfile>visual__error__system_8c.html</anchorfile>
      <anchor>a32b1c50c1082ad40d9f9f13fd28cedd7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>web_server_task.h</name>
    <path>components/web_server_task/include/</path>
    <filename>web__server__task_8h.html</filename>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <member kind="enumeration">
      <type></type>
      <name>web_command_type_t</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a2c7befe5d1c5ab38fe478aae4ae86684</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>WEB_CMD_START_SERVER</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a2c7befe5d1c5ab38fe478aae4ae86684aad9719bd0826f4a39d8295eff4581e7f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>WEB_CMD_STOP_SERVER</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a2c7befe5d1c5ab38fe478aae4ae86684a7538e0d87ed15a4ba791f6b29c76ac0e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>WEB_CMD_GET_STATUS</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a2c7befe5d1c5ab38fe478aae4ae86684aeb03bffc76583b8d76e33a9e964003b0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>WEB_CMD_SET_CONFIG</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a2c7befe5d1c5ab38fe478aae4ae86684a6f0d3db11bea649d9b2921e7aee9dc92</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_task_start</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a706a239d45aec91411ba21ec2fe1b6f9</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_process_commands</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>aee13c30e5ba3aacbdb4adf76ac429f08</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_execute_command</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>ae84a23138d1c5c146c9cadf884edaa85</anchor>
      <arglist>(uint8_t command)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_start</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a1449ade4d0e7558ec4fdb355d19b2d05</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_stop</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a0d3daaa268fa6003470a334af7dbd2c1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_get_status</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a46cb4e2e81f7578c9f230f5a3bcd11e9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_set_config</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a46d1719629fd536b25bd288a334d338c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_update_state</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a64790e5afc3435f96af702391bf6a24d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_handle_root</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a483e27ca5d114ebfc2d8e6ea723bb804</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_handle_api_status</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>acd8c0cf4c727a56b30cf72b6b48edcb4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_handle_api_board</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a3bba3e089bedf1e0e30e5e4cd1db5091</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_handle_api_move</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a4680b3adbb255bf863bcf5f81925b618</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_websocket_init</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a362473905ca28dd32196ec327d9ccaee</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_websocket_send_update</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a2cb5b9ee72f8acc55f6cb8f8b45f8bec</anchor>
      <arglist>(const char *data)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>web_server_is_active</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a29aa411112dd288259022706b7ff59ca</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>web_server_get_client_count</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>ab47f0c904798d9ffc4f8e75f7ce8b5f3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>web_server_get_uptime</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>aed7d37015ce007d6f84d759c94ad5f26</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_log_request</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>af8feae54c5bb7a79bc36e947985c7e9f</anchor>
      <arglist>(const char *method, const char *path)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_log_error</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a81f3502fd237eb59ce2ebfc3999b9d36</anchor>
      <arglist>(const char *error_message)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_set_port</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>ab18a2857822cb150c96fa252d484246a</anchor>
      <arglist>(uint16_t port)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_set_max_clients</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>ad915360105d028b5ebd3ca1b38e391d4</anchor>
      <arglist>(uint32_t max_clients)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_enable_ssl</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a74937acaf240abcaaac7d9260e412beb</anchor>
      <arglist>(bool enable)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>web_server_is_task_running</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>aa8fab275f60059ceb3bc58df7650bf0b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_stop_task</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a76a1b24d8a4f73007def0e2675cb2b1c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_reset</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>ae0743c305f029ddda532f3823d8bd602</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>wifi_save_config_to_nvs</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>acc62bf6d95aba71aede977266f96e962</anchor>
      <arglist>(const char *ssid, const char *password)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>wifi_connect_sta</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>ad3bda523e632b9d3152cdd73fe9d8d4c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>wifi_disconnect_sta</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a4a4813aac1f99fb986c17f6a4f5e40e2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>wifi_clear_config_from_nvs</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a1b7f75613a5cab50704428a000610531</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>wifi_is_sta_connected</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a96cbfc09f923b4548a4bb4c63f70237f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>web_is_locked</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>add53c7f34376bb856c868cab6321070b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>web_lock_set</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a325753fb6460ffaa94c0c32237c8e7e1</anchor>
      <arglist>(bool locked)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>web_lock_load_from_nvs</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>afd6d2236bd4fcbbe7ccbbb0e7b4a71de</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>wifi_load_config_from_nvs</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a0915923195ef83ed0a4a8ab10cfe3d24</anchor>
      <arglist>(char *ssid, size_t ssid_len, char *password, size_t password_len)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>wifi_get_sta_ip</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a2fb41e5bc5236e57873eb55f1eadb51c</anchor>
      <arglist>(char *buffer, size_t max_len)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>wifi_get_sta_ssid</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>a0dd2f78dc29e5c5cd031dcdf59d587e6</anchor>
      <arglist>(char *buffer, size_t max_len)</arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>web_server_status_queue</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>aa17b405ee779d94d3b6954dd83712591</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>web_server_command_queue</name>
      <anchorfile>web__server__task_8h.html</anchorfile>
      <anchor>aaed4d7c5bc3e8140853498b965ec94e2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>web_server_task.c</name>
    <path>components/web_server_task/</path>
    <filename>web__server__task_8c.html</filename>
    <includes id="web__server__task_8h" name="web_server_task.h" local="yes" import="no" module="no" objc="no">web_server_task.h</includes>
    <includes id="game__task_8h" name="game_task.h" local="yes" import="no" module="no" objc="no">../game_task/include/game_task.h</includes>
    <includes id="ha__light__task_8h" name="ha_light_task.h" local="yes" import="no" module="no" objc="no">../ha_light_task/include/ha_light_task.h</includes>
    <includes id="led__task_8h" name="led_task.h" local="yes" import="no" module="no" objc="no">../led_task/include/led_task.h</includes>
    <includes id="led__mapping_8h" name="led_mapping.h" local="yes" import="no" module="no" objc="no">led_mapping.h</includes>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>WIFI_AP_SSID</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a8e19d13af09f7ed0afad391730f1e3e7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>WIFI_AP_PASSWORD</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ac80c92af8bf29cfe0442a28b6a7ec5a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>WIFI_AP_CHANNEL</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ac0a1dba58f0e6d7f07d3050fa0291dd0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>WIFI_AP_MAX_CONNECTIONS</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ab689d29cda2d350ce6652141df530fd0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>WIFI_AP_IP</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a067a8d9ec14859ad6237320d2cec686a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>WIFI_AP_GATEWAY</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a2782b50f9a156696a1708c61823aa3a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>WIFI_AP_NETMASK</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>af48472f62a4bab9896e181b25d439f34</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>WIFI_NVS_NAMESPACE</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a3be9b58c528a64824c7b4847f5589837</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>WIFI_NVS_KEY_SSID</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a4d51acbc4a1127232436f69fbbc18683</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>WIFI_NVS_KEY_PASSWORD</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a434b6722326d8ef2da75d4d215ac6992</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>WEB_NVS_NAMESPACE</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a94e74a2c614fc3d5ea5f639a55f2221b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>WEB_NVS_KEY_LOCKED</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a34ccd19a2d18b64a7db37e39c68dc704</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HTTP_SERVER_PORT</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a4f905ddd4da1a3e205a8935f4babb25b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HTTP_SERVER_MAX_URI_LEN</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ac6dafba2315f7dd8ef7954d67c1c58ef</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HTTP_SERVER_MAX_HEADERS</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aa471f425e7c4a8154dd0a6aef7b1c151</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HTTP_SERVER_MAX_CLIENTS</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a8fed04375198615d8723433210c7861a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>JSON_BUFFER_SIZE</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a2e4ba70a12e464d5b5278c18d28355f8</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>web_server_task_wdt_reset_safe</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a6c97a0ccfbc6eb7b4c5fc2443d19a974</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>toggle_demo_mode</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a6e05201e522856e3cd984bade768be7e</anchor>
      <arglist>(bool enabled)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_demo_speed_ms</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a570c98f23cd91c36ce62964733732cdf</anchor>
      <arglist>(uint32_t speed_ms)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>is_demo_mode_enabled</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ad190bb08e29e63b6f04a3715dae88706</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>wifi_init_apsta</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a67b85e21ee1a283f1f8ba6b818e6bb3e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>wifi_event_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ac31e38637f7a5e5c8ce4d033b0955fec</anchor>
      <arglist>(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>start_http_server</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a336ec184b985a0af307c5ad81f06ae9e</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>stop_http_server</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a12a90512dcfbad1de2475bba47af2c18</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_get_root_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ac4be9f5032c10525d0b61493e402b758</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_get_chess_js_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>afeadcd80458be0b8f189b25986dc9e72</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_get_test_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ae7ac8a7695f85cdf9c15b8045b2813b3</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_get_favicon_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a9c15047ba3df7b2591894a68c0d80923</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_get_board_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a1df4df74a1d4c242f385b8288f5243d4</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_get_status_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>abe969da6ebebc1b67cb06e9fc8bf3125</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_get_history_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ad3a55001d3de8d4f584d7c64bed5f13d</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_get_captured_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a97ff386657b49592624586a42a5f38e4</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_get_advantage_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aeac0e653e9a5ab503812d90dac9623d5</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_get_timer_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ab7d5385d7f569007c56c2c7a7fa7d6c3</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_timer_config_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a475710334bf70a033c8877844b4becf7</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_timer_pause_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a908aa1beee83594820b7c14d65c18278</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_timer_resume_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a25c5ac166fb8596946684e3e3f9e0202</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_timer_reset_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a3500d692620d566066b284e7678946c6</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_game_move_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aeb0c7739faebc37cc1a058f796ba24e9</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_wifi_config_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a6493232eb46ff49eb44d894c8c4a1daf</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_wifi_connect_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a3d0bd2243c20416d59e0fba4fcd733a6</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_wifi_disconnect_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a1e666336ebbd7c13672804089a310f19</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_wifi_clear_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a55bff9badf4ff599d4b2a8e272f9f318</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_get_wifi_status_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aedaea596294dea3bc9a8670d696e8290</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_get_web_lock_status_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a860d44d17a7928a2ad3d09628635dd31</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_demo_config_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a94ab4e3ed0c9d94e819d7e4e9e66b9d8</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_get_demo_status_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a1b2ca6e53cae11465e829b142b4ee049</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_game_virtual_action_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>af36e989f916f7fc74dfbe1bbf52b3869</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_game_new_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aa9ed8d6dbc130fd18b7d409f4f742189</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_game_hint_highlight_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>afe04c688d88ea0650555b298507ea154</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_get_mqtt_status_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a540c54cb24f48beccd6d626d89d25267</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_mqtt_config_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a93314f062ff3d38822815ee5f6c2761d</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>wifi_load_config_from_nvs</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a0915923195ef83ed0a4a8ab10cfe3d24</anchor>
      <arglist>(char *ssid, size_t ssid_len, char *password, size_t password_len)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>wifi_get_sta_status_json</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>adf48bce5b2fe5cafb51a5a5b928ca31d</anchor>
      <arglist>(char *buffer, size_t buffer_size)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>wifi_get_sta_ip</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a2fb41e5bc5236e57873eb55f1eadb51c</anchor>
      <arglist>(char *buffer, size_t max_len)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>wifi_get_sta_ssid</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a0dd2f78dc29e5c5cd031dcdf59d587e6</anchor>
      <arglist>(char *buffer, size_t max_len)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>wifi_save_config_to_nvs</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>acc62bf6d95aba71aede977266f96e962</anchor>
      <arglist>(const char *ssid, const char *password)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>wifi_connect_sta</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ad3bda523e632b9d3152cdd73fe9d8d4c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>wifi_disconnect_sta</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a4a4813aac1f99fb986c17f6a4f5e40e2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>wifi_clear_config_from_nvs</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a1b7f75613a5cab50704428a000610531</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>wifi_is_sta_connected</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a96cbfc09f923b4548a4bb4c63f70237f</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>web_lock_save_to_nvs</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a7a944cacc1ac5f2f8077a9244aaa44e3</anchor>
      <arglist>(bool locked)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>web_lock_load_from_nvs</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>afd6d2236bd4fcbbe7ccbbb0e7b4a71de</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>web_is_locked</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>add53c7f34376bb856c868cab6321070b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>web_lock_set</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a325753fb6460ffaa94c0c32237c8e7e1</anchor>
      <arglist>(bool locked)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_settings_brightness_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aa2b42e517ee6eb9665e1d10deb00c4ad</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>http_post_demo_start_handler</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a35259616c06dac2e639ca26124109c70</anchor>
      <arglist>(httpd_req_t *req)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>__wrap_esp_log_writev</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a9aed8f59f75d856e2916530518eff586</anchor>
      <arglist>(esp_log_level_t level, const char *tag, const char *format, va_list args)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>__wrap_esp_log_write</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ac16f4afbf3b99fa63b395c4f03e720d8</anchor>
      <arglist>(esp_log_level_t level, const char *tag, const char *format,...)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_task_start</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a706a239d45aec91411ba21ec2fe1b6f9</anchor>
      <arglist>(void *pvParameters)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_process_commands</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aee13c30e5ba3aacbdb4adf76ac429f08</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_execute_command</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ae84a23138d1c5c146c9cadf884edaa85</anchor>
      <arglist>(uint8_t command)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_start</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a1449ade4d0e7558ec4fdb355d19b2d05</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_stop</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a0d3daaa268fa6003470a334af7dbd2c1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_get_status</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a46cb4e2e81f7578c9f230f5a3bcd11e9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_set_config</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a46d1719629fd536b25bd288a334d338c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_update_state</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a64790e5afc3435f96af702391bf6a24d</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_handle_root</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a483e27ca5d114ebfc2d8e6ea723bb804</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_handle_api_status</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>acd8c0cf4c727a56b30cf72b6b48edcb4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_handle_api_board</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a3bba3e089bedf1e0e30e5e4cd1db5091</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_handle_api_move</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a4680b3adbb255bf863bcf5f81925b618</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_websocket_init</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a362473905ca28dd32196ec327d9ccaee</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_websocket_send_update</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a2cb5b9ee72f8acc55f6cb8f8b45f8bec</anchor>
      <arglist>(const char *data)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>web_server_is_active</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a29aa411112dd288259022706b7ff59ca</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>web_server_get_client_count</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ab47f0c904798d9ffc4f8e75f7ce8b5f3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>web_server_get_uptime</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aed7d37015ce007d6f84d759c94ad5f26</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_log_request</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>af8feae54c5bb7a79bc36e947985c7e9f</anchor>
      <arglist>(const char *method, const char *path)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_log_error</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a81f3502fd237eb59ce2ebfc3999b9d36</anchor>
      <arglist>(const char *error_message)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_set_port</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ab18a2857822cb150c96fa252d484246a</anchor>
      <arglist>(uint16_t port)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_set_max_clients</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ad915360105d028b5ebd3ca1b38e391d4</anchor>
      <arglist>(uint32_t max_clients)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_enable_ssl</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a74937acaf240abcaaac7d9260e412beb</anchor>
      <arglist>(bool enable)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>web_server_is_task_running</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aa8fab275f60059ceb3bc58df7650bf0b</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_stop_task</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a76a1b24d8a4f73007def0e2675cb2b1c</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>web_server_reset</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ae0743c305f029ddda532f3823d8bd602</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>game_command_queue</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a4ed770cbe46604586e494bae84b62304</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>task_running</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a35141cb45623cb3b7b4d96da35ed1831</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>web_server_active</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aafa0fef29f6314e439892920011c0612</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>wifi_ap_active</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a6fd14c6866c5be5170e78061b5af1cca</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>web_server_start_time</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a78e57b763d02a6d2f0e9e96a9bd4bdc4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>client_count</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a21f294411b76ec3adac8af9c73e83b13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static httpd_handle_t</type>
      <name>httpd_handle</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a19b0be42f085a9e121651e3a21813ce8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static esp_netif_t *</type>
      <name>ap_netif</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a91aeff1220701b1ecdc74ab211de8243</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static esp_netif_t *</type>
      <name>sta_netif</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a8475209c3d16d25790fc4b67e18f3f57</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>sta_connected</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a738f5dc6c12f15f0149e08de59f48064</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>sta_connecting</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ae88cda9a42f3f54cda581d8f3df6a981</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>web_locked</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aa21e15af227cd88b231854eef4d9a1cc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>sta_ip</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aea67e706fb77d2b28047d62252b54088</anchor>
      <arglist>[16]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static char</type>
      <name>sta_ssid</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a8e71a432a4ef4bac3ead0d19184bad9e</anchor>
      <arglist>[33]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static int</type>
      <name>last_disconnect_reason</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ae6a105bb6087d94dc1dff9c49c17a3d8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint8_t</type>
      <name>cached_brightness</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ab17ad7844a24ac3ce03f648f18a3c976</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static bool</type>
      <name>cached_brightness_valid</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a10437ead7390b9e656788a7546bb15b2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>web_server_status_queue</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aa17b405ee779d94d3b6954dd83712591</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>web_server_command_queue</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aaed4d7c5bc3e8140853498b965ec94e2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static char</type>
      <name>json_buffer</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ab9623bd189e3125138776d88fc5d35ff</anchor>
      <arglist>[2048]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>chess_app_js_content</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a31bf285a16bbf718a86c71d94f2b9f31</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>test_html</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a42e66ee24fe5081421583e97925b3d7a</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_head</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a50be2765f30c79691d319bc9a1b6920c</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_layout_start</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ad8c6a6333babf87707dc21080b71f726</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_tab_hra_start</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a84a7f8dd86a306e95f345b80a4940b1e</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_game_left</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ace9a7067ecc97f7c7756a562c9b5c1c0</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_game_right</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a4ed2739c81538da574348a3db94e6462</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_tab_hra_end</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a0dc3a9e01e0ee61af3f276badf9efc09</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_board</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a0075c9a2ed7f41df9d5f1e62ee82ff80</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_tab_nastaveni_start</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ad572c26c328220889e67688a5399ce65</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_settings_sit</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ae190241b4afa6e796e3537850ccbbdc7</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_settings_zarizeni</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aff807476fdafba6db3514a20fa7374cb</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_banners</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a38326283e81a476ac89426aea7cf3250</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_hint_inline</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>adc3003a9441c86d367006ae4415a82b4</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_javascript</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>ad68d8dce5b4bb5eb53f9201c4a146c2f</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_demo_mode</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a8bf9680c8995edd413cb2da4a543c3d6</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_mqtt_config</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>aed0fb8deb128d87225f755340b40201e</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_tab_nastaveni_end</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a3e25ed064bdf023c8829e45eea11ea4d</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char</type>
      <name>html_chunk_end</name>
      <anchorfile>web__server__task_8c.html</anchorfile>
      <anchor>a3d913a16031d5b9a41a6dc6551e6ce61</anchor>
      <arglist>[]</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>main.c</name>
    <path>main/</path>
    <filename>main_8c.html</filename>
    <includes id="animation__task_8h" name="animation_task.h" local="yes" import="no" module="no" objc="no">animation_task.h</includes>
    <includes id="button__task_8h" name="button_task.h" local="yes" import="no" module="no" objc="no">button_task.h</includes>
    <includes id="chess__types_8h" name="chess_types.h" local="yes" import="no" module="no" objc="no">chess_types.h</includes>
    <includes id="freertos__chess_8h" name="freertos_chess.h" local="yes" import="no" module="no" objc="no">freertos_chess.h</includes>
    <includes id="game__task_8h" name="game_task.h" local="yes" import="no" module="no" objc="no">game_task.h</includes>
    <includes id="led__task_8h" name="led_task.h" local="yes" import="no" module="no" objc="no">led_task.h</includes>
    <includes id="matrix__task_8h" name="matrix_task.h" local="yes" import="no" module="no" objc="no">matrix_task.h</includes>
    <includes id="test__task_8h" name="test_task.h" local="yes" import="no" module="no" objc="no">test_task.h</includes>
    <includes id="uart__task_8h" name="uart_task.h" local="yes" import="no" module="no" objc="no">uart_task.h</includes>
    <includes id="game__led__animations_8h" name="game_led_animations.h" local="yes" import="no" module="no" objc="no">game_led_animations.h</includes>
    <includes id="ha__light__task_8h" name="ha_light_task.h" local="yes" import="no" module="no" objc="no">ha_light_task.h</includes>
    <includes id="uart__commands__extended_8h" name="uart_commands_extended.h" local="yes" import="no" module="no" objc="no">uart_commands_extended.h</includes>
    <includes id="web__server__task_8h" name="web_server_task.h" local="yes" import="no" module="no" objc="no">web_server_task.h</includes>
    <member kind="function" static="yes">
      <type>static const char *</type>
      <name>reset_reason_to_str</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>abb182517d44ccd021b3dac9355989c7b</anchor>
      <arglist>(esp_reset_reason_t reason)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>show_boot_animation_and_board</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a4894ad0ac4d99cffa56d58649a1850fb</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static esp_err_t</type>
      <name>main_task_wdt_reset_safe</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a16454398c460e4d7161a90c808248ec2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>demo_report_activity</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a768b8bf362b42a4e04ac58639035a938</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>main_system_init</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>ae27993c1756124d9fa6e8af4958409ef</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>initialize_chess_game</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>acf3129796410e265806bde4edacf4d85</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>toggle_demo_mode</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a6e05201e522856e3cd984bade768be7e</anchor>
      <arglist>(bool enabled)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>is_demo_mode_enabled</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>ad190bb08e29e63b6f04a3715dae88706</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_demo_speed_ms</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a570c98f23cd91c36ce62964733732cdf</anchor>
      <arglist>(uint32_t speed_ms)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>execute_demo_move</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a47be0f08c5acbba3a901be7d65e4cb8a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>init_console</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>ae52825df5837e0f8f5e814f48cafb722</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>esp_err_t</type>
      <name>create_system_tasks</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a2dbcd7c01e5197da615bb4fe6c5b29b0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>app_main</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a630544a7f0a2cc40d8a7fefab7e2fe70</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>TAG</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a5a85b9c772bbeb480b209a3e6ea92b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SemaphoreHandle_t</type>
      <name>uart_mutex</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a8eca84d3e2c7c0adf364819e74b1bd9b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>led_task_handle</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a6cf81c896b094e20ff5752ee1133a8f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>matrix_task_handle</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a9de1b87e0b6692c90f1c5cfd9e13d444</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>button_task_handle</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>aba5c2db71f9fa5cd9f3e506e91ffe6d3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>uart_task_handle</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a4e4967ac70f1e8a89b2fea62027a968c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>game_task_handle</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>acb49a688d90460dc48f7e0b976ace62d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>animation_task_handle</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a2a20898222a990efbce3e4239f2d9aee</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>test_task_handle</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>abafefd1ba465b609ca39fd6fc502095b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>web_server_task_handle</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a36e9302901a63037025cd7970fa8adbe</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>ha_light_task_handle</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a5c597bf4c3d1fff34c3ef12254f0efc4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>reset_button_task_handle</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>aeb8dd1b563b914d04c90f377454ad032</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>promotion_button_task_handle</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a2cf065fd2e74d6ad91b3da384a96c60b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static volatile bool</type>
      <name>demo_mode_enabled</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>af252e203098e091d068b85d637b1083d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static uint32_t</type>
      <name>current_demo_delay_ms</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>aa999d9fce86efd55be4fb1bce525e69e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>DEMO_GAME_WHITE_WIN</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>acc98f8280521aceb9893120eba837382</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>DEMO_GAME_OPERA</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a22d3cef2fa6060633fc71190b5d6df3b</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char **</type>
      <name>DEMO_GAMES</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>ac2614434acc93da191dda0a8a445e651</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>DEMO_GAME_LENGTHS</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>abcaca94aeee21681707035e985a0c843</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char *</type>
      <name>DEMO_GAME_NAMES</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a167e2d80b3314581d6120d1b2e48ee51</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>DEMO_GAMES_COUNT</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a66025d47b9e143dea05d41ea2a76d46b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static int</type>
      <name>current_demo_game</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a19159754c1693faa54833d370834e343</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static int</type>
      <name>demo_move_index</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>aebfac3059b619dc182e9ad6011fe1c7b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const char **</type>
      <name>current_demo_moves</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a9cd35e669cb2fd803a99504ba0da1aaa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static int</type>
      <name>demo_moves_count</name>
      <anchorfile>main_8c.html</anchorfile>
      <anchor>a4973ab5b9a06d7ed486bc100be691f42</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>README.md</name>
    <path></path>
    <filename>README_8md.html</filename>
  </compound>
  <compound kind="struct">
    <name>active_error_t</name>
    <filename>structactive__error__t.html</filename>
    <member kind="variable">
      <type>visual_error_type_t</type>
      <name>type</name>
      <anchorfile>structactive__error__t.html</anchorfile>
      <anchor>a0084c8ccc5ca684603c69d4c7bb56852</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>id</name>
      <anchorfile>structactive__error__t.html</anchorfile>
      <anchor>a86b4c2d315025f29e9406e5a7eb3cfb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>active</name>
      <anchorfile>structactive__error__t.html</anchorfile>
      <anchor>a9fc4f1a331d5ef481e74dda7d47026b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>start_time</name>
      <anchorfile>structactive__error__t.html</anchorfile>
      <anchor>a321bf7ba51c11b3061c4faf665463e4b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>animation_id</name>
      <anchorfile>structactive__error__t.html</anchorfile>
      <anchor>a240a5c11f613516aa8e3816f64350ff6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>row</name>
      <anchorfile>structactive__error__t.html</anchorfile>
      <anchor>a3a1ae9add15aea94b873af99aeb85509</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>col</name>
      <anchorfile>structactive__error__t.html</anchorfile>
      <anchor>adaa2a257182c4b9ca438a9bb2cf79db8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>user_confirmed</name>
      <anchorfile>structactive__error__t.html</anchorfile>
      <anchor>ace7dcdd59cad912bc07b3fc6c4fdb007</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>animation_config_t</name>
    <filename>structanimation__config__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>max_concurrent_animations</name>
      <anchorfile>structanimation__config__t.html</anchorfile>
      <anchor>a23691ea10ec49cd03cc0ec7d1d24c052</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>update_frequency_hz</name>
      <anchorfile>structanimation__config__t.html</anchorfile>
      <anchor>aedd96ef71c0b884f50fcb1a69e0ee024</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>enable_smooth_interpolation</name>
      <anchorfile>structanimation__config__t.html</anchorfile>
      <anchor>a74ff887373a2bd67b497e6e74e7f3e86</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>enable_trail_effects</name>
      <anchorfile>structanimation__config__t.html</anchorfile>
      <anchor>ae14543413a4b18a194570af26986ff82</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>default_duration_ms</name>
      <anchorfile>structanimation__config__t.html</anchorfile>
      <anchor>a06a10b970ebd4a1ece687dda50d2026a</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>animation_state_struct</name>
    <filename>structanimation__state__struct.html</filename>
    <member kind="variable">
      <type>uint32_t</type>
      <name>id</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a732e0dbd6e57d26471ce3f333a133f2c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>animation_type_t</type>
      <name>type</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>ac79573e97685e638f9c2b5cd71812866</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>animation_priority_t</type>
      <name>priority</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a8e140ccb4b01f66009817c3eabb3cfdb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>active</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>ab589c6dea9706219b895760e5000b84c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>looping</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a464a2373685a8d9ffd904fe653d93948</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>start_time</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>ae0a72eacc1d16227ba69fba467a0529e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>duration_ms</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a9570f68952cc32df071946e4fac9882e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>current_frame</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a6cd808b11cdf192b14cb1c6bae1aa2bb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>progress</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a1442eda3168d67833881a91d25b43e63</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>from_led</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>abb19daa0d3794443fa78e3eb363ed314</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>to_led</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a0704478ccab727055e9776a96a8a9af2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>center_led</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>ae43d0e2f19d7e6218a15ab7ea4b3fa64</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>trail_length</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a279664f2466e44e92fb9cb065d13acc3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>from_row</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a7cde404deed44276640c337ff89c9a94</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>from_col</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a7d7b84971c84f2be81d1d16b8ed861a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>to_row</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a048f97179d87aaef0f177f47235842fc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>to_col</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a5ba94fece824afa2309f549e66e00ea3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>affected_positions</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a33119ff945aa1d9a0e172f5a26fea14f</anchor>
      <arglist>[64]</arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>affected_count</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a11dd74ed0b510b921d3c87adc0f78aee</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>r</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a81ba55cfe9c438efcb2f3a01ba0c250d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>g</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a44d6d80720ab776fc0e69bcc89af7998</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>b</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a54ad728f789eaa216561ab83c8281b18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>struct animation_state_struct::@142164266231324065324063215162302310256115247206</type>
      <name>color_start</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a2922ea68742f00aa170aec907413a09a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>struct animation_state_struct::@014121250062057242316216164044241372207133312321</type>
      <name>color_end</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a38f0e16a77fd8363c7b777bae1b634f0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>struct animation_state_struct::@364104262077140336145337175044306122344034255274</type>
      <name>color_primary</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a3c902fd4518851b7f8a180675924d140</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>struct animation_state_struct::@234200203173077142047242270000117031356161066275</type>
      <name>color_secondary</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>ae4c860baf9947da18d8e8b7f7fb6805e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>speed</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a9a52073808958a49d056dfe7ae67a626</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>intensity</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a077ad0cbcd3a0f3419280fa9dd1189c9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>winner_color</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a9979b435eb6e089f4695566d3e3c1a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool(*</type>
      <name>update_func</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>ab1a5f1afbbdb0e1a7cf3eb90ddc29206</anchor>
      <arglist>)(animation_state_t *anim)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>on_complete</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a1257c5b14f68eec69cc6a480895748d0</anchor>
      <arglist>)(uint32_t id)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>on_frame</name>
      <anchorfile>structanimation__state__struct.html</anchorfile>
      <anchor>a5596162671dcc10a67f1eeeb102da26a</anchor>
      <arglist>)(uint32_t id, uint32_t frame)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>animation_task_t</name>
    <filename>structanimation__task__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>id</name>
      <anchorfile>structanimation__task__t.html</anchorfile>
      <anchor>adb7f1e556a2073bf7ce347ee3f96cfe4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>animation_task_state_t</type>
      <name>state</name>
      <anchorfile>structanimation__task__t.html</anchorfile>
      <anchor>abffffba2feaf4ae9d6911cc115a91593</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>active</name>
      <anchorfile>structanimation__task__t.html</anchorfile>
      <anchor>abc3e62a06682ed4652bf1ed358c84f0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>paused</name>
      <anchorfile>structanimation__task__t.html</anchorfile>
      <anchor>a4323d7e401e5a856bcdbef5c57503653</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>loop</name>
      <anchorfile>structanimation__task__t.html</anchorfile>
      <anchor>a567b61ac057e886362023c7363f4aefd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>priority</name>
      <anchorfile>structanimation__task__t.html</anchorfile>
      <anchor>a5ffc03906acb15f1e4528c668bc41778</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>animation_task_type_t</type>
      <name>type</name>
      <anchorfile>structanimation__task__t.html</anchorfile>
      <anchor>ac98d14c78132155465672c4943857a2a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>start_time</name>
      <anchorfile>structanimation__task__t.html</anchorfile>
      <anchor>a25f85a5720a2bd7add2c2423f84cbee4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>duration_ms</name>
      <anchorfile>structanimation__task__t.html</anchorfile>
      <anchor>a4c5211bfc199b4bdbc596849387d05ae</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>frame_duration_ms</name>
      <anchorfile>structanimation__task__t.html</anchorfile>
      <anchor>a9302e023c296097ae10d831469dc25f7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>current_frame</name>
      <anchorfile>structanimation__task__t.html</anchorfile>
      <anchor>a49e9a3dd3d14920942a194b2b658ee01</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>total_frames</name>
      <anchorfile>structanimation__task__t.html</anchorfile>
      <anchor>a273fbd2a2ffa5590a455c97ac1c1f09b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>frame_interval</name>
      <anchorfile>structanimation__task__t.html</anchorfile>
      <anchor>ac770e95f6dafd7507d61a359cdc53ce8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>data</name>
      <anchorfile>structanimation__task__t.html</anchorfile>
      <anchor>a0b2f1d684a7d68b707b1372684d1e9f2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>buffer_pool_stats_t</name>
    <filename>structbuffer__pool__stats__t.html</filename>
    <member kind="variable">
      <type>uint32_t</type>
      <name>pool_size</name>
      <anchorfile>structbuffer__pool__stats__t.html</anchorfile>
      <anchor>ab36127065062a985fffa4951c5b033b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>buffer_size</name>
      <anchorfile>structbuffer__pool__stats__t.html</anchorfile>
      <anchor>a8273dd396e87398156b713e2770687cc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>current_usage</name>
      <anchorfile>structbuffer__pool__stats__t.html</anchorfile>
      <anchor>a42990bc34dd3c40cfb396f6aa3723bbc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>peak_usage</name>
      <anchorfile>structbuffer__pool__stats__t.html</anchorfile>
      <anchor>ae2d19a60f60e93b6b4144919fa1ef156</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>total_allocations</name>
      <anchorfile>structbuffer__pool__stats__t.html</anchorfile>
      <anchor>add17a4e17bdbbc10a693d74443db4325</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>total_releases</name>
      <anchorfile>structbuffer__pool__stats__t.html</anchorfile>
      <anchor>a0bc82e1e4f4f20bc8f3cf31fc0d0148c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>allocation_failures</name>
      <anchorfile>structbuffer__pool__stats__t.html</anchorfile>
      <anchor>a36dc96032c4fe870bc181bbbb9fdb2dd</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>button_event_t</name>
    <filename>structbutton__event__t.html</filename>
    <member kind="variable">
      <type>button_event_type_t</type>
      <name>type</name>
      <anchorfile>structbutton__event__t.html</anchorfile>
      <anchor>aaf4df16f9df04bfcd820337024e7dec6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>button_id</name>
      <anchorfile>structbutton__event__t.html</anchorfile>
      <anchor>a42509e77a8d1f6fe1cafa0705f27b6df</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>press_duration_ms</name>
      <anchorfile>structbutton__event__t.html</anchorfile>
      <anchor>a2ff3c3470b9574501f65c5ac0676f240</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>timestamp</name>
      <anchorfile>structbutton__event__t.html</anchorfile>
      <anchor>a4f1d453477528540280683b13b83446d</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>castling_error_info_t</name>
    <filename>structcastling__error__info__t.html</filename>
    <member kind="variable">
      <type>castling_error_t</type>
      <name>error_type</name>
      <anchorfile>structcastling__error__info__t.html</anchorfile>
      <anchor>acb3a3313d18dedd85e669edb5f493bd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>description</name>
      <anchorfile>structcastling__error__info__t.html</anchorfile>
      <anchor>acd904d8c841b637d1d3d311472d84e8e</anchor>
      <arglist>[128]</arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>error_led_positions</name>
      <anchorfile>structcastling__error__info__t.html</anchorfile>
      <anchor>ab4dcce8a4f74078bf52a19211c137c74</anchor>
      <arglist>[8]</arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>correction_led_positions</name>
      <anchorfile>structcastling__error__info__t.html</anchorfile>
      <anchor>a6b18152768ec2ff1295e705b10a74b18</anchor>
      <arglist>[8]</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>recovery_action</name>
      <anchorfile>structcastling__error__info__t.html</anchorfile>
      <anchor>a3fa688388a68b217d091d9f92a49e8bd</anchor>
      <arglist>)(void)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>castling_led_config_t</name>
    <filename>structcastling__led__config__t.html</filename>
    <member kind="variable">
      <type>rgb_color_t</type>
      <name>king_highlight</name>
      <anchorfile>structcastling__led__config__t.html</anchorfile>
      <anchor>a1fe1b245d36f081d19965a5289f9f712</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>rgb_color_t</type>
      <name>king_destination</name>
      <anchorfile>structcastling__led__config__t.html</anchorfile>
      <anchor>af99e940507d3ef2e8fb16664fd167360</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>rgb_color_t</type>
      <name>rook_highlight</name>
      <anchorfile>structcastling__led__config__t.html</anchorfile>
      <anchor>a82da172dc989a50b96d81778b283e067</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>rgb_color_t</type>
      <name>rook_destination</name>
      <anchorfile>structcastling__led__config__t.html</anchorfile>
      <anchor>acce48927878d078580e8acc3e47294dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>rgb_color_t</type>
      <name>error_indication</name>
      <anchorfile>structcastling__led__config__t.html</anchorfile>
      <anchor>aa84e4a577f5bc80b788921512546b748</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>rgb_color_t</type>
      <name>path_guidance</name>
      <anchorfile>structcastling__led__config__t.html</anchorfile>
      <anchor>a09f718a9ce213215e3d661ffd636a879</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>struct castling_led_config_t::@172265366106067321172063244375254225100213240333</type>
      <name>colors</name>
      <anchorfile>structcastling__led__config__t.html</anchorfile>
      <anchor>abb030c5d0841242d7878ae92a3ff6c31</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>pulsing_speed</name>
      <anchorfile>structcastling__led__config__t.html</anchorfile>
      <anchor>a94c260b9495c4ff37f896d47baea1f9a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>guidance_speed</name>
      <anchorfile>structcastling__led__config__t.html</anchorfile>
      <anchor>a6c699c147c9888fb2919a2fb790277a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>error_flash_count</name>
      <anchorfile>structcastling__led__config__t.html</anchorfile>
      <anchor>a12575b4c400d92d567187cdf9b5127a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>completion_celebration_duration</name>
      <anchorfile>structcastling__led__config__t.html</anchorfile>
      <anchor>a0478760a2694c3ca13222f3e64083b03</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>struct castling_led_config_t::@226022330271045236162175310217275141151043275015</type>
      <name>timing</name>
      <anchorfile>structcastling__led__config__t.html</anchorfile>
      <anchor>a9126cd142c5d8a01981e4c13ae3fd62d</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>castling_led_state_t</name>
    <filename>structcastling__led__state__t.html</filename>
    <member kind="variable">
      <type>uint32_t</type>
      <name>king_animation_id</name>
      <anchorfile>structcastling__led__state__t.html</anchorfile>
      <anchor>acc27bb99a6cbb794b94854f40d593c59</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>rook_animation_id</name>
      <anchorfile>structcastling__led__state__t.html</anchorfile>
      <anchor>a31ff62b630a954e7384df452efdf94f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>guidance_animation_id</name>
      <anchorfile>structcastling__led__state__t.html</anchorfile>
      <anchor>a6a52813b439afa5496bd6351ba351954</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>showing_error</name>
      <anchorfile>structcastling__led__state__t.html</anchorfile>
      <anchor>a0d3a81d2075158d32cc067fc2aa6d1c5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>showing_guidance</name>
      <anchorfile>structcastling__led__state__t.html</anchorfile>
      <anchor>af2bc6f8611d243b8d9cee39e4353bfec</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>castling_positions_t</name>
    <filename>structcastling__positions__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>king_from_row</name>
      <anchorfile>structcastling__positions__t.html</anchorfile>
      <anchor>a8f8646113529baeede5db86bbca7b759</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>king_from_col</name>
      <anchorfile>structcastling__positions__t.html</anchorfile>
      <anchor>a61a8cebe1265bd5c314ede822850874d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>king_to_row</name>
      <anchorfile>structcastling__positions__t.html</anchorfile>
      <anchor>af10c6d54bf7046e73a6a0d5d48a88bf9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>king_to_col</name>
      <anchorfile>structcastling__positions__t.html</anchorfile>
      <anchor>a6b2b0a29876d269219f706fa857eb78e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>rook_from_row</name>
      <anchorfile>structcastling__positions__t.html</anchorfile>
      <anchor>a268786a1222cb336eec41510ac089f01</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>rook_from_col</name>
      <anchorfile>structcastling__positions__t.html</anchorfile>
      <anchor>a5bf47d2b25a7365cfada2110d6c375f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>rook_to_row</name>
      <anchorfile>structcastling__positions__t.html</anchorfile>
      <anchor>a327b358b5219956a1a4c1a17cadb9363</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>rook_to_col</name>
      <anchorfile>structcastling__positions__t.html</anchorfile>
      <anchor>a56dd6d90404022b71164de6f9b02b88d</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>castling_state_t</name>
    <filename>structcastling__state__t.html</filename>
    <member kind="variable">
      <type>bool</type>
      <name>in_progress</name>
      <anchorfile>structcastling__state__t.html</anchorfile>
      <anchor>a1705abfdc6a05d4bec2293871b7b7fd1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>rook_from_row</name>
      <anchorfile>structcastling__state__t.html</anchorfile>
      <anchor>a5487d6de95e717db3a49bf0ed4aed5fd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>rook_from_col</name>
      <anchorfile>structcastling__state__t.html</anchorfile>
      <anchor>afde7ce6130b7476c3bc926c09e267d10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>rook_to_row</name>
      <anchorfile>structcastling__state__t.html</anchorfile>
      <anchor>a4744be90f6da3461a00f214616844512</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>rook_to_col</name>
      <anchorfile>structcastling__state__t.html</anchorfile>
      <anchor>a3f6d980f0e18ee2f5df5907b82d500a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>player_t</type>
      <name>player</name>
      <anchorfile>structcastling__state__t.html</anchorfile>
      <anchor>ac32cc33e24d53fff7f4bca524ceceb1d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_kingside</name>
      <anchorfile>structcastling__state__t.html</anchorfile>
      <anchor>aae2796ad5c3e40911c0c4002c77190ff</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>chess_move_command_t</name>
    <filename>structchess__move__command__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>type</name>
      <anchorfile>structchess__move__command__t.html</anchorfile>
      <anchor>a87f810beeb0b0322d2b99c87d5a7c91c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>from_notation</name>
      <anchorfile>structchess__move__command__t.html</anchorfile>
      <anchor>a51b8eb6617ef0642c722008827f2d3d1</anchor>
      <arglist>[8]</arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>to_notation</name>
      <anchorfile>structchess__move__command__t.html</anchorfile>
      <anchor>a3396c5f2d92b68852cb9fcf6e79500fc</anchor>
      <arglist>[8]</arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>player</name>
      <anchorfile>structchess__move__command__t.html</anchorfile>
      <anchor>a431384d6ef7f4db67d9a0946a7211ef1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>response_queue</name>
      <anchorfile>structchess__move__command__t.html</anchorfile>
      <anchor>a750e92bc6c9a40c005e3e48947dd10b5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>promotion_choice</name>
      <anchorfile>structchess__move__command__t.html</anchorfile>
      <anchor>ae6e4449e8345fed34c9c685eba9f1363</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>time_control_type</name>
      <anchorfile>structchess__move__command__t.html</anchorfile>
      <anchor>a19b5d713be6ea325b1121180f0382263</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>custom_minutes</name>
      <anchorfile>structchess__move__command__t.html</anchorfile>
      <anchor>ab1469d3613ac521e6521da4c7fabad75</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>custom_increment</name>
      <anchorfile>structchess__move__command__t.html</anchorfile>
      <anchor>aa66efcd3b24e2c130ede000aca79466d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>struct chess_move_command_t::@233046107143104020212156201337246223060226357106::@253257030051145302203130262005111067225123267170</type>
      <name>timer_config</name>
      <anchorfile>structchess__move__command__t.html</anchorfile>
      <anchor>ae05888f64383039d1414ffa36ae22aa9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_white_turn</name>
      <anchorfile>structchess__move__command__t.html</anchorfile>
      <anchor>a7ee2cb22f123d144e42a6a0a6eb6d05a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>struct chess_move_command_t::@233046107143104020212156201337246223060226357106::@113272302233052360371047066147155221030321032135</type>
      <name>timer_state</name>
      <anchorfile>structchess__move__command__t.html</anchorfile>
      <anchor>af494be6080aeefa370736fedfe0eb0c5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>union chess_move_command_t::@233046107143104020212156201337246223060226357106</type>
      <name>timer_data</name>
      <anchorfile>structchess__move__command__t.html</anchorfile>
      <anchor>a2e078f65d1fab8a9ebd51547cd733fd9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_demo_mode</name>
      <anchorfile>structchess__move__command__t.html</anchorfile>
      <anchor>a43bd07ae3571e5c911bb94292a4cf6b3</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>chess_move_extended_t</name>
    <filename>structchess__move__extended__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>from_row</name>
      <anchorfile>structchess__move__extended__t.html</anchorfile>
      <anchor>a7e73820058cbbef9a1cfa47dfd1e5f9e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>from_col</name>
      <anchorfile>structchess__move__extended__t.html</anchorfile>
      <anchor>a8ff4fcb8d2a94dfbc8f099a4815534c8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>to_row</name>
      <anchorfile>structchess__move__extended__t.html</anchorfile>
      <anchor>a287d533872003ff749138ed16e12d299</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>to_col</name>
      <anchorfile>structchess__move__extended__t.html</anchorfile>
      <anchor>aa5a0ffb4e21787a7ad0b6843b296ceda</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>piece_t</type>
      <name>piece</name>
      <anchorfile>structchess__move__extended__t.html</anchorfile>
      <anchor>aec665ac464d3a9223022d74795a4a8ab</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>piece_t</type>
      <name>captured_piece</name>
      <anchorfile>structchess__move__extended__t.html</anchorfile>
      <anchor>a16faacd78ba6e8d0036fe058422762d0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>move_type_t</type>
      <name>move_type</name>
      <anchorfile>structchess__move__extended__t.html</anchorfile>
      <anchor>ae6200dd0d0fe9610ed5ee1de3d410aa0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>promotion_choice_t</type>
      <name>promotion_piece</name>
      <anchorfile>structchess__move__extended__t.html</anchorfile>
      <anchor>ad18a1ba58a42a71b7004dc78a654a0dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>timestamp</name>
      <anchorfile>structchess__move__extended__t.html</anchorfile>
      <anchor>a446b69d07624770d7f9caed4a8f9c01a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_check</name>
      <anchorfile>structchess__move__extended__t.html</anchorfile>
      <anchor>ab73d86e56bd5ce7bae6cc0029f8ab1e4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_checkmate</name>
      <anchorfile>structchess__move__extended__t.html</anchorfile>
      <anchor>a888c1191497c06e0485a65c7a5b8223a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_stalemate</name>
      <anchorfile>structchess__move__extended__t.html</anchorfile>
      <anchor>a64548a3d3b994e79cfddb1ff7ec47680</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>chess_move_t</name>
    <filename>structchess__move__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>from_row</name>
      <anchorfile>structchess__move__t.html</anchorfile>
      <anchor>a87e6934d4b28fea1a59c341d6cbe9eb7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>from_col</name>
      <anchorfile>structchess__move__t.html</anchorfile>
      <anchor>a3b8bdb363c16e1f221da6c3e80d9aa70</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>to_row</name>
      <anchorfile>structchess__move__t.html</anchorfile>
      <anchor>a12ee5250e947928c028f50b4c28c9d24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>to_col</name>
      <anchorfile>structchess__move__t.html</anchorfile>
      <anchor>a481848c592fd4bcac9b17b995ac18de1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>piece_t</type>
      <name>piece</name>
      <anchorfile>structchess__move__t.html</anchorfile>
      <anchor>abedc85077eb8fc089e9b2e71d5761354</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>piece_t</type>
      <name>captured_piece</name>
      <anchorfile>structchess__move__t.html</anchorfile>
      <anchor>a02d91c42164e16fe1bb058dd71f04b8e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>timestamp</name>
      <anchorfile>structchess__move__t.html</anchorfile>
      <anchor>af891d8ac7c690204fc9a1ff9f302457c</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>chess_position_t</name>
    <filename>structchess__position__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>row</name>
      <anchorfile>structchess__position__t.html</anchorfile>
      <anchor>a52fc42938f588120d7427968dd5c6c5b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>col</name>
      <anchorfile>structchess__position__t.html</anchorfile>
      <anchor>a3c084704decce0bc27213aff418814dc</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>chess_timer_t</name>
    <filename>structchess__timer__t.html</filename>
    <member kind="variable">
      <type>uint32_t</type>
      <name>white_time_ms</name>
      <anchorfile>structchess__timer__t.html</anchorfile>
      <anchor>a7d0e2964acf12e9046f18f07e99486c4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>black_time_ms</name>
      <anchorfile>structchess__timer__t.html</anchorfile>
      <anchor>a258578da63390a476c98ed0cee25867c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>move_start_time</name>
      <anchorfile>structchess__timer__t.html</anchorfile>
      <anchor>a7f7510da4fac73ade5242b40627e62c5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>last_move_time</name>
      <anchorfile>structchess__timer__t.html</anchorfile>
      <anchor>ac77301ca2e97bf1a65e71ea92937a893</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>timer_running</name>
      <anchorfile>structchess__timer__t.html</anchorfile>
      <anchor>a31819afd556ed4a6266abee25bef7f70</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_white_turn</name>
      <anchorfile>structchess__timer__t.html</anchorfile>
      <anchor>a5fbbe88e66de5ea14c6570f5526a39ae</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>game_paused</name>
      <anchorfile>structchess__timer__t.html</anchorfile>
      <anchor>ac4990048c62805c9cec772b9d7005abe</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>time_expired</name>
      <anchorfile>structchess__timer__t.html</anchorfile>
      <anchor>a7389bbc57384b8534999549a9f36b9f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>time_control_config_t</type>
      <name>config</name>
      <anchorfile>structchess__timer__t.html</anchorfile>
      <anchor>a07b9f7d20041000000ac867e286454e5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>total_moves</name>
      <anchorfile>structchess__timer__t.html</anchorfile>
      <anchor>a2821d53bca4e86476b744bc2fb372461</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>avg_move_time_ms</name>
      <anchorfile>structchess__timer__t.html</anchorfile>
      <anchor>a9f1982f8c62719b0b75a2f0c5147c0aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>warning_30s_shown</name>
      <anchorfile>structchess__timer__t.html</anchorfile>
      <anchor>ad20283280bacabc85f2662834c6e659b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>warning_10s_shown</name>
      <anchorfile>structchess__timer__t.html</anchorfile>
      <anchor>a3509cb6dc20f8a0fd54d869fb90e69f8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>warning_5s_shown</name>
      <anchorfile>structchess__timer__t.html</anchorfile>
      <anchor>ac73b4959948d7f77c5cff4d9dcd671ca</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>command_history_t</name>
    <filename>structcommand__history__t.html</filename>
    <member kind="variable">
      <type>char</type>
      <name>commands</name>
      <anchorfile>structcommand__history__t.html</anchorfile>
      <anchor>a1e73944c26e45b42a860dd6ed004a097</anchor>
      <arglist>[20][256]</arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>current</name>
      <anchorfile>structcommand__history__t.html</anchorfile>
      <anchor>a3f60e1de0c721ff7a5f70b67639f6d8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>count</name>
      <anchorfile>structcommand__history__t.html</anchorfile>
      <anchor>aaa72fe8503aab11ebc8d7b697865a6ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_size</name>
      <anchorfile>structcommand__history__t.html</anchorfile>
      <anchor>abec7bb35589530cfa72fb075ecd4f151</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>endgame_wave_state_t</name>
    <filename>structendgame__wave__state__t.html</filename>
    <member kind="variable">
      <type>bool</type>
      <name>active</name>
      <anchorfile>structendgame__wave__state__t.html</anchorfile>
      <anchor>a4816664cc0fad0ddab256fde1c68561d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>win_king_led</name>
      <anchorfile>structendgame__wave__state__t.html</anchorfile>
      <anchor>a33ad93583f7aa05aab9e22119c93fd1b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>win_king_row</name>
      <anchorfile>structendgame__wave__state__t.html</anchorfile>
      <anchor>a915432d954a16e72e099867cfcfd10ca</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>win_king_col</name>
      <anchorfile>structendgame__wave__state__t.html</anchorfile>
      <anchor>a242e59ba36a19f566a9e8aab498406bb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>lose_king_row</name>
      <anchorfile>structendgame__wave__state__t.html</anchorfile>
      <anchor>a329a347d4dfd1cdfb862d3a4a0eb4b4b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>lose_king_col</name>
      <anchorfile>structendgame__wave__state__t.html</anchorfile>
      <anchor>a9863524a43454de68ee5dbcb12f55b71</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>radius</name>
      <anchorfile>structendgame__wave__state__t.html</anchorfile>
      <anchor>a17e93286f25caf7283e221863bd27e85</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>last_update</name>
      <anchorfile>structendgame__wave__state__t.html</anchorfile>
      <anchor>a5da1a65206e58ad2a8e9124000233e32</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>initialized</name>
      <anchorfile>structendgame__wave__state__t.html</anchorfile>
      <anchor>ab0ac52042b1175b54102095a79d802cd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>piece_t</type>
      <name>winner_piece</name>
      <anchorfile>structendgame__wave__state__t.html</anchorfile>
      <anchor>a27d3a2de8932f03f1fd0d4e75c3b0858</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>enhanced_castling_system_t</name>
    <filename>structenhanced__castling__system__t.html</filename>
    <member kind="variable">
      <type>castling_phase_t</type>
      <name>phase</name>
      <anchorfile>structenhanced__castling__system__t.html</anchorfile>
      <anchor>a9ad143ea4c1df172843b282169a4dfde</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>active</name>
      <anchorfile>structenhanced__castling__system__t.html</anchorfile>
      <anchor>a193693f084c9983b7d3631df4ca11cf0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>player_t</type>
      <name>player</name>
      <anchorfile>structenhanced__castling__system__t.html</anchorfile>
      <anchor>abe1e8b371d2cd4a14f44bc619ba239c9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_kingside</name>
      <anchorfile>structenhanced__castling__system__t.html</anchorfile>
      <anchor>aed1696fbc7c2f7f72e5b8488abc901fd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>castling_positions_t</type>
      <name>positions</name>
      <anchorfile>structenhanced__castling__system__t.html</anchorfile>
      <anchor>ad8d7f5951f47c93f5e4af53e4c6374a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>phase_start_time</name>
      <anchorfile>structenhanced__castling__system__t.html</anchorfile>
      <anchor>a578c27a1cbedf763948800fd7331b635</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>phase_timeout_ms</name>
      <anchorfile>structenhanced__castling__system__t.html</anchorfile>
      <anchor>acdf875b3cc914c109c760c56f7fff55d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>error_count</name>
      <anchorfile>structenhanced__castling__system__t.html</anchorfile>
      <anchor>af58209be6b886146ebd983684f6e339e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>max_errors</name>
      <anchorfile>structenhanced__castling__system__t.html</anchorfile>
      <anchor>a0907be6b43008863a3914d90dea75b3b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>castling_led_state_t</type>
      <name>led_state</name>
      <anchorfile>structenhanced__castling__system__t.html</anchorfile>
      <anchor>a2a0f299e6a721d6a6f7c9e6d80dc7a35</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>completion_callback</name>
      <anchorfile>structenhanced__castling__system__t.html</anchorfile>
      <anchor>ab9d558418c635e3e56021273d508f6a1</anchor>
      <arglist>)(bool success)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>error_guidance_t</name>
    <filename>structerror__guidance__t.html</filename>
    <member kind="variable">
      <type>visual_error_type_t</type>
      <name>error_type</name>
      <anchorfile>structerror__guidance__t.html</anchorfile>
      <anchor>ac976af5d4ebbc9c8edd138a093ec47fa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>error_led_positions</name>
      <anchorfile>structerror__guidance__t.html</anchorfile>
      <anchor>a05ef7e185aab6b2b28af271f494c600e</anchor>
      <arglist>[16]</arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>error_led_count</name>
      <anchorfile>structerror__guidance__t.html</anchorfile>
      <anchor>ac608c1b3b734493a96d191545f28588d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>guidance_led_positions</name>
      <anchorfile>structerror__guidance__t.html</anchorfile>
      <anchor>a9b8f41cb28f4f557077f038ee332930c</anchor>
      <arglist>[16]</arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>guidance_led_count</name>
      <anchorfile>structerror__guidance__t.html</anchorfile>
      <anchor>a939552c50553b9eb9aff6d0a86af9d8b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>user_message</name>
      <anchorfile>structerror__guidance__t.html</anchorfile>
      <anchor>a2d4d5d7f00c4613b12fd59a5e48b123c</anchor>
      <arglist>[128]</arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>recovery_hint</name>
      <anchorfile>structerror__guidance__t.html</anchorfile>
      <anchor>aadbc3531a0c7adcf1a4878cac4d67b85</anchor>
      <arglist>[128]</arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>led_positions</name>
      <anchorfile>structerror__guidance__t.html</anchorfile>
      <anchor>a4fa20765026297cd67e7d82b721ebced</anchor>
      <arglist>[16]</arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>led_count</name>
      <anchorfile>structerror__guidance__t.html</anchorfile>
      <anchor>a82e76fbaa6a870491132d7fe5505d6c4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>display_duration_ms</name>
      <anchorfile>structerror__guidance__t.html</anchorfile>
      <anchor>a092d951b1b44d718a23eb4aa9de2a407</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>require_user_confirm</name>
      <anchorfile>structerror__guidance__t.html</anchorfile>
      <anchor>a14400cf1fab2b94d126d753a649bf1e2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>error_system_config_t</name>
    <filename>structerror__system__config__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>flash_count</name>
      <anchorfile>structerror__system__config__t.html</anchorfile>
      <anchor>ab9332345cfa6e3584a5e148399845955</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>flash_duration_ms</name>
      <anchorfile>structerror__system__config__t.html</anchorfile>
      <anchor>aac1d5a5146938b908431709b41a5d2eb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>guidance_duration_ms</name>
      <anchorfile>structerror__system__config__t.html</anchorfile>
      <anchor>a84028f31dbc291ebeaeb4616fa3618e9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>enable_recovery_hints</name>
      <anchorfile>structerror__system__config__t.html</anchorfile>
      <anchor>a915b397389b11a215d78f2cb802ec815</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>max_concurrent_errors</name>
      <anchorfile>structerror__system__config__t.html</anchorfile>
      <anchor>aa21a28fe433e260b41b5af893c4aae9b</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>firework_t</name>
    <filename>structfirework__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>center_x</name>
      <anchorfile>structfirework__t.html</anchorfile>
      <anchor>a81ca0f04b4c781a1f799eb96a3395009</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>center_y</name>
      <anchorfile>structfirework__t.html</anchorfile>
      <anchor>a37330f2a3b0e7da717cfccb0912b8cfd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>radius</name>
      <anchorfile>structfirework__t.html</anchorfile>
      <anchor>a25ee75b0312399808b4c1aa7d36cdacb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>max_radius</name>
      <anchorfile>structfirework__t.html</anchorfile>
      <anchor>a6ed1b298267dbca24bf65e40f7661b24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>color_idx</name>
      <anchorfile>structfirework__t.html</anchorfile>
      <anchor>a01940dbfdce44ef9a569aaa7820353fd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>active</name>
      <anchorfile>structfirework__t.html</anchorfile>
      <anchor>ab8dfb88348a42289cd5ce7a31b40c77b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>delay</name>
      <anchorfile>structfirework__t.html</anchorfile>
      <anchor>accc9b17020e87c721a531237aa0b78c3</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>game_response_t</name>
    <filename>structgame__response__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>type</name>
      <anchorfile>structgame__response__t.html</anchorfile>
      <anchor>a3002d679599149eb947d042adf7d28a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>command_type</name>
      <anchorfile>structgame__response__t.html</anchorfile>
      <anchor>a94891c48651d30f6ab358f269e789130</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>error_code</name>
      <anchorfile>structgame__response__t.html</anchorfile>
      <anchor>a9b46f74a2c06621c440e9cd5bfd1c04e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>message</name>
      <anchorfile>structgame__response__t.html</anchorfile>
      <anchor>ae4afd8e408f8ec3943b6f8632b1333a7</anchor>
      <arglist>[256]</arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>data</name>
      <anchorfile>structgame__response__t.html</anchorfile>
      <anchor>a1b9907db84187372f7e5df9135e8d522</anchor>
      <arglist>[64]</arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>timestamp</name>
      <anchorfile>structgame__response__t.html</anchorfile>
      <anchor>a6ca9b6d2062bf38fa5e6c8fb01a3ca04</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ha_light_command_t</name>
    <filename>structha__light__command__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>type</name>
      <anchorfile>structha__light__command__t.html</anchorfile>
      <anchor>aa655a0a61f12692c31c68803ae070fcf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>data</name>
      <anchorfile>structha__light__command__t.html</anchorfile>
      <anchor>ab0bc8ed3f5ca6cb30316a8163e8da184</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>input_buffer_t</name>
    <filename>structinput__buffer__t.html</filename>
    <member kind="variable">
      <type>char</type>
      <name>buffer</name>
      <anchorfile>structinput__buffer__t.html</anchorfile>
      <anchor>a610a358b649f0993a69f3f5fe1ae885b</anchor>
      <arglist>[256]</arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>pos</name>
      <anchorfile>structinput__buffer__t.html</anchorfile>
      <anchor>a49d086081ae7176f57ff7ee4f5564375</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>length</name>
      <anchorfile>structinput__buffer__t.html</anchorfile>
      <anchor>a46a4cd3407d6ca0eadb41800b7e52e54</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>cursor_visible</name>
      <anchorfile>structinput__buffer__t.html</anchorfile>
      <anchor>a2dd4c5a97c85c19099c1696e5083e69c</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>king_resignation_state_t</name>
    <filename>structking__resignation__state__t.html</filename>
    <member kind="variable">
      <type>bool</type>
      <name>active</name>
      <anchorfile>structking__resignation__state__t.html</anchorfile>
      <anchor>a1186d33a02c7b828ad090842484f8f8c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>player_t</type>
      <name>player</name>
      <anchorfile>structking__resignation__state__t.html</anchorfile>
      <anchor>aae1c259379dddc3435700259e5bebe8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>king_row</name>
      <anchorfile>structking__resignation__state__t.html</anchorfile>
      <anchor>a8aa2d198aefd1c851caa794ed1c1af41</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>king_col</name>
      <anchorfile>structking__resignation__state__t.html</anchorfile>
      <anchor>a37e62616ed1e01e5461649b7fbe074e2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TimerHandle_t</type>
      <name>main_timer</name>
      <anchorfile>structking__resignation__state__t.html</anchorfile>
      <anchor>ad466ba868c959c1683c6d3a8545380d0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TimerHandle_t</type>
      <name>animation_timer</name>
      <anchorfile>structking__resignation__state__t.html</anchorfile>
      <anchor>a1c717d8def9f7fdab3de11b94f78fd8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint64_t</type>
      <name>start_time_ms</name>
      <anchorfile>structking__resignation__state__t.html</anchorfile>
      <anchor>a3405dda1390652be995ba6af7ad4db56</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>last_countdown_sec</name>
      <anchorfile>structking__resignation__state__t.html</anchorfile>
      <anchor>aaf4fec46a7bd11bc464a3a3b22d21c9d</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>led_animation_state_t</name>
    <filename>structled__animation__state__t.html</filename>
    <member kind="variable">
      <type>bool</type>
      <name>is_active</name>
      <anchorfile>structled__animation__state__t.html</anchorfile>
      <anchor>a87fea1e33aa73c51d4a62b16e317ce7c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>animation_type</name>
      <anchorfile>structled__animation__state__t.html</anchorfile>
      <anchor>a0f6462d7758bbbe06d2ce2f2e9afabc8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>current_frame</name>
      <anchorfile>structled__animation__state__t.html</anchorfile>
      <anchor>ae0449ec5d8c12b23b647aabd6abac77a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>total_frames</name>
      <anchorfile>structled__animation__state__t.html</anchorfile>
      <anchor>ae1e66294a9876ce015ec1c03e9aa7ddc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>frame_duration_ms</name>
      <anchorfile>structled__animation__state__t.html</anchorfile>
      <anchor>a10114c4e3964d053486143e4d1b848dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>last_update_time</name>
      <anchorfile>structled__animation__state__t.html</anchorfile>
      <anchor>af2a9ad7a9d458c2c3cbb5c17c2bed39d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>source_square</name>
      <anchorfile>structled__animation__state__t.html</anchorfile>
      <anchor>adec8168c3173a7ed92dc6a00aa9211d7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>target_square</name>
      <anchorfile>structled__animation__state__t.html</anchorfile>
      <anchor>ac88657853a4666cd6bc861a4ae2b8a75</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>color_r</name>
      <anchorfile>structled__animation__state__t.html</anchorfile>
      <anchor>aecd9be7048baa443db3554414befd371</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>color_g</name>
      <anchorfile>structled__animation__state__t.html</anchorfile>
      <anchor>a28829e2c6bb0831613cf5075e845e3e1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>color_b</name>
      <anchorfile>structled__animation__state__t.html</anchorfile>
      <anchor>ae70ab943e9cd6f98a372be2bcd18404c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>interrupt_on_placement</name>
      <anchorfile>structled__animation__state__t.html</anchorfile>
      <anchor>a2e62ade81888805e7bc4e2262e1f2336</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>led_color_t</name>
    <filename>structled__color__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>r</name>
      <anchorfile>structled__color__t.html</anchorfile>
      <anchor>a371fbf1cdc3048a5c69fd727c2945761</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>g</name>
      <anchorfile>structled__color__t.html</anchorfile>
      <anchor>acb23349611560c14c762d951a03a732c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>b</name>
      <anchorfile>structled__color__t.html</anchorfile>
      <anchor>a2f586742abe86e32f633fbcefaf9db9d</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>led_command_t</name>
    <filename>structled__command__t.html</filename>
    <member kind="variable">
      <type>led_command_type_t</type>
      <name>type</name>
      <anchorfile>structled__command__t.html</anchorfile>
      <anchor>a7665b1fa37b249a3377af43862c1040b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>led_index</name>
      <anchorfile>structled__command__t.html</anchorfile>
      <anchor>ae1ef770e3412d70be11bb43b7d2736ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>red</name>
      <anchorfile>structled__command__t.html</anchorfile>
      <anchor>a6465f3443696b28dbff862b0cd143257</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>green</name>
      <anchorfile>structled__command__t.html</anchorfile>
      <anchor>ac0ef067ae80b28a440ca65558ff3a9e0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>blue</name>
      <anchorfile>structled__command__t.html</anchorfile>
      <anchor>a79b80c2980e53738a06f92d305bba3a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>duration_ms</name>
      <anchorfile>structled__command__t.html</anchorfile>
      <anchor>a42fe1a9d7b5330a035a60be952b37dfc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>data</name>
      <anchorfile>structled__command__t.html</anchorfile>
      <anchor>a8fb54529dc345b2f804899e68a519163</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>response_queue</name>
      <anchorfile>structled__command__t.html</anchorfile>
      <anchor>a4c27aa01b250ef21991565d3876cc24f</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>led_duration_state_t</name>
    <filename>structled__duration__state__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>led_index</name>
      <anchorfile>structled__duration__state__t.html</anchorfile>
      <anchor>a5eb4bd5f09eca3583663e668d4b74122</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>original_color</name>
      <anchorfile>structled__duration__state__t.html</anchorfile>
      <anchor>a8aba5c7b290cef110bf2e11ee4c7c1e6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>duration_color</name>
      <anchorfile>structled__duration__state__t.html</anchorfile>
      <anchor>a390cee9413644684b60bb6b498e33649</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>start_time</name>
      <anchorfile>structled__duration__state__t.html</anchorfile>
      <anchor>a46e946dc43ba12c21ab7796f8cec03f8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>duration_ms</name>
      <anchorfile>structled__duration__state__t.html</anchorfile>
      <anchor>a8b55f6df0e7276a5d94274a711324271</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_active</name>
      <anchorfile>structled__duration__state__t.html</anchorfile>
      <anchor>aeea3959322066cfecc1c29ed8cb514b5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>restore_original</name>
      <anchorfile>structled__duration__state__t.html</anchorfile>
      <anchor>a3171dae77eb54657c6c137ce3fc014fa</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>led_frame_buffer_t</name>
    <filename>structled__frame__buffer__t.html</filename>
    <member kind="variable">
      <type>uint32_t</type>
      <name>frame_id</name>
      <anchorfile>structled__frame__buffer__t.html</anchorfile>
      <anchor>ad59373747e464a6f577533997ee29925</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>timestamp</name>
      <anchorfile>structled__frame__buffer__t.html</anchorfile>
      <anchor>aada4e392b9241225098f16e1cd279943</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_complete_frame</name>
      <anchorfile>structled__frame__buffer__t.html</anchorfile>
      <anchor>a8da6dbd33973ef157754bfa2640f30ff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>led_count</name>
      <anchorfile>structled__frame__buffer__t.html</anchorfile>
      <anchor>a72527fb151eaff14d33b0c7528fc0368</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>pixels</name>
      <anchorfile>structled__frame__buffer__t.html</anchorfile>
      <anchor>af58f8d92807a4ebe06ce6135f5dc30b3</anchor>
      <arglist>[(64+9)]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>led_health_stats_t</name>
    <filename>structled__health__stats__t.html</filename>
    <member kind="variable">
      <type>uint32_t</type>
      <name>commands_processed</name>
      <anchorfile>structled__health__stats__t.html</anchorfile>
      <anchor>a9a0f4f67b88467369c5b56f008ae4436</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>commands_failed</name>
      <anchorfile>structled__health__stats__t.html</anchorfile>
      <anchor>a64315595cf52748458fbdb44a29293ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>mutex_timeouts</name>
      <anchorfile>structled__health__stats__t.html</anchorfile>
      <anchor>a0b88add05c5826b2ae3d9058187f0444</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>frame_drops</name>
      <anchorfile>structled__health__stats__t.html</anchorfile>
      <anchor>a77bac0504651661760667dab1de0b5dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>hardware_errors</name>
      <anchorfile>structled__health__stats__t.html</anchorfile>
      <anchor>ac6ba5967b998bd74c35dab640990cca9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>last_update_time</name>
      <anchorfile>structled__health__stats__t.html</anchorfile>
      <anchor>a71c3bd70b6692d20cbb64e95e44411a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>success_rate</name>
      <anchorfile>structled__health__stats__t.html</anchorfile>
      <anchor>a7c240ea2565141afb036406f2fdd86bb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>average_frame_time_ms</name>
      <anchorfile>structled__health__stats__t.html</anchorfile>
      <anchor>a7d9e95f1afd0b1b48c7863b1d382d685</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>min_frame_time_ms</name>
      <anchorfile>structled__health__stats__t.html</anchorfile>
      <anchor>a34c491347abf9321e57c5d59e6e7314e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>max_frame_time_ms</name>
      <anchorfile>structled__health__stats__t.html</anchorfile>
      <anchor>ab08c0f3da9d85647f6c69d621a6be06c</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>led_layer_state_t</name>
    <filename>structled__layer__state__t.html</filename>
    <member kind="variable">
      <type>led_pixel_t</type>
      <name>pixels</name>
      <anchorfile>structled__layer__state__t.html</anchorfile>
      <anchor>ae8835f14508990758c1933e4fb5f8c41</anchor>
      <arglist>[73]</arglist>
    </member>
    <member kind="variable">
      <type>blend_mode_t</type>
      <name>blend_mode</name>
      <anchorfile>structled__layer__state__t.html</anchorfile>
      <anchor>a751d0f8893d44627cbe70a38cecf353b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>enabled</name>
      <anchorfile>structled__layer__state__t.html</anchorfile>
      <anchor>aa9705fa7af03de520f1494dc7b209615</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>layer_enabled</name>
      <anchorfile>structled__layer__state__t.html</anchorfile>
      <anchor>a8495dd827b717049ad56be88bf2eafa5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>master_alpha</name>
      <anchorfile>structled__layer__state__t.html</anchorfile>
      <anchor>a112df6c248bc73c864b6e0c401a1a87c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>layer_opacity</name>
      <anchorfile>structled__layer__state__t.html</anchorfile>
      <anchor>a2a85c68f064e0ea31442bf6a2ad2bb13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>dirty</name>
      <anchorfile>structled__layer__state__t.html</anchorfile>
      <anchor>a238ba32530dd3a543bde13518ad7edd1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>needs_composite</name>
      <anchorfile>structled__layer__state__t.html</anchorfile>
      <anchor>ae5d91895551c0ec81b39011bcc7a07ff</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>led_manager_config_t</name>
    <filename>structled__manager__config__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>max_brightness</name>
      <anchorfile>structled__manager__config__t.html</anchorfile>
      <anchor>a322873aea9fecdd3975347d7312ebe7f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>default_brightness</name>
      <anchorfile>structled__manager__config__t.html</anchorfile>
      <anchor>a5efa2115bc7071dcc1bb22f7ccdc8aa0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>enable_smooth_transitions</name>
      <anchorfile>structled__manager__config__t.html</anchorfile>
      <anchor>ab0ecc3ff79210c341d0e60ca1988387c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>enable_layer_compositing</name>
      <anchorfile>structled__manager__config__t.html</anchorfile>
      <anchor>a5c1ddd074f9089bac82dbba7ef07ca44</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>update_frequency_hz</name>
      <anchorfile>structled__manager__config__t.html</anchorfile>
      <anchor>aeb8bda8e64d5460b28a2f12f3b59d33f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>transition_duration_ms</name>
      <anchorfile>structled__manager__config__t.html</anchorfile>
      <anchor>aa788666f88b8ef6f4130d9dc08b2d3bd</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>led_pixel_t</name>
    <filename>structled__pixel__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>r</name>
      <anchorfile>structled__pixel__t.html</anchorfile>
      <anchor>afd559e1dbe5d5b0e95df5c1a71ac673e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>g</name>
      <anchorfile>structled__pixel__t.html</anchorfile>
      <anchor>aacb643b10ce86ae94cbecf21bc9b9b4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>b</name>
      <anchorfile>structled__pixel__t.html</anchorfile>
      <anchor>a45671018a6c9259bb3986ff7fdd75453</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>alpha</name>
      <anchorfile>structled__pixel__t.html</anchorfile>
      <anchor>ab07803a676dbdf8c8f7f0387f577edf4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>brightness</name>
      <anchorfile>structled__pixel__t.html</anchorfile>
      <anchor>a5cb1f7787f6504a80d57c3dbe4da536b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>dirty</name>
      <anchorfile>structled__pixel__t.html</anchorfile>
      <anchor>a43b55b0c8710014028abd706ff815082</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>last_update</name>
      <anchorfile>structled__pixel__t.html</anchorfile>
      <anchor>a007c8afc62dec2058eb60610a89fbdce</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>led_state_t</name>
    <filename>structled__state__t.html</filename>
    <member kind="variable">
      <type>led_layer_state_t</type>
      <name>layers</name>
      <anchorfile>structled__state__t.html</anchorfile>
      <anchor>a28f07abcdc7ee3321597cffc5ea8411d</anchor>
      <arglist>[LED_LAYER_COUNT]</arglist>
    </member>
    <member kind="variable">
      <type>led_pixel_t</type>
      <name>composite</name>
      <anchorfile>structled__state__t.html</anchorfile>
      <anchor>afc5601630c57ab8013c4edd741cd319f</anchor>
      <arglist>[64]</arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>dirty_pixels</name>
      <anchorfile>structled__state__t.html</anchorfile>
      <anchor>ac3040efe9cad342ad5a6474de2467157</anchor>
      <arglist>[64]</arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>needs_update</name>
      <anchorfile>structled__state__t.html</anchorfile>
      <anchor>a2d6fa87d252e94ba3acf105b8f33eeae</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>last_update_time</name>
      <anchorfile>structled__state__t.html</anchorfile>
      <anchor>a8a87e46e82ac9a67e08c5daf17afdf46</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SemaphoreHandle_t</type>
      <name>mutex</name>
      <anchorfile>structled__state__t.html</anchorfile>
      <anchor>a90d938528de22889c8fe760576c40c3d</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>matrix_command_t</name>
    <filename>structmatrix__command__t.html</filename>
    <member kind="variable">
      <type>matrix_command_type_t</type>
      <name>type</name>
      <anchorfile>structmatrix__command__t.html</anchorfile>
      <anchor>a3a27afd6b95921539849e3a6e0f81a2d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>data</name>
      <anchorfile>structmatrix__command__t.html</anchorfile>
      <anchor>a4b1b3566cdf60e6cb30905c134a48ff9</anchor>
      <arglist>[16]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>matrix_event_t</name>
    <filename>structmatrix__event__t.html</filename>
    <member kind="variable">
      <type>matrix_event_type_t</type>
      <name>type</name>
      <anchorfile>structmatrix__event__t.html</anchorfile>
      <anchor>a136d126a221d1e83577bdec39d611382</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>from_square</name>
      <anchorfile>structmatrix__event__t.html</anchorfile>
      <anchor>a2f07c2c41a4eb95ffb933402aa485de6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>to_square</name>
      <anchorfile>structmatrix__event__t.html</anchorfile>
      <anchor>ab9ad1ee61d7f911f987a7da34798a2c7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>piece_t</type>
      <name>piece_type</name>
      <anchorfile>structmatrix__event__t.html</anchorfile>
      <anchor>a8a605943957a1d906ebbb3180b630cf7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>timestamp</name>
      <anchorfile>structmatrix__event__t.html</anchorfile>
      <anchor>afabbf043d2cf02d953194fc388c00ad4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>from_row</name>
      <anchorfile>structmatrix__event__t.html</anchorfile>
      <anchor>a2f644f5eb6efda19f98cf82398d63f1f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>from_col</name>
      <anchorfile>structmatrix__event__t.html</anchorfile>
      <anchor>a74960ff8c206f3c997800987d93f0ab0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>to_row</name>
      <anchorfile>structmatrix__event__t.html</anchorfile>
      <anchor>a9dd9d3bb07f54ed5d020900ee4b4b965</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>to_col</name>
      <anchorfile>structmatrix__event__t.html</anchorfile>
      <anchor>a995368e0b41ee11bac2b47da35499757</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>move_suggestion_t</name>
    <filename>structmove__suggestion__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>from_row</name>
      <anchorfile>structmove__suggestion__t.html</anchorfile>
      <anchor>a940eca031248f8df49031c483af30f13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>from_col</name>
      <anchorfile>structmove__suggestion__t.html</anchorfile>
      <anchor>a5805172608c1e85601ee674644b69fe7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>to_row</name>
      <anchorfile>structmove__suggestion__t.html</anchorfile>
      <anchor>a81af2edcc02d5fef8b48fb1c8fe22134</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>to_col</name>
      <anchorfile>structmove__suggestion__t.html</anchorfile>
      <anchor>a68a17ae426e3f81008e87b1f92090935</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>piece_t</type>
      <name>piece</name>
      <anchorfile>structmove__suggestion__t.html</anchorfile>
      <anchor>a95bfdc4963cc77122f941663554a8a67</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_capture</name>
      <anchorfile>structmove__suggestion__t.html</anchorfile>
      <anchor>a58f2cd5416633410aebfa8f80320e3aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_check</name>
      <anchorfile>structmove__suggestion__t.html</anchorfile>
      <anchor>a39763b8b278db010f5dfff7a4beb264a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_castling</name>
      <anchorfile>structmove__suggestion__t.html</anchorfile>
      <anchor>aa9a5da748efa93feb7e3f7c4f06e7839</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_en_passant</name>
      <anchorfile>structmove__suggestion__t.html</anchorfile>
      <anchor>a77114011b2edb38b0218c36ce67cf0e6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>score</name>
      <anchorfile>structmove__suggestion__t.html</anchorfile>
      <anchor>a96c798a72d6ed66a18c8b2d1e840e604</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>non_blocking_blink_state_t</name>
    <filename>structnon__blocking__blink__state__t.html</filename>
    <member kind="variable">
      <type>bool</type>
      <name>active</name>
      <anchorfile>structnon__blocking__blink__state__t.html</anchorfile>
      <anchor>ab2f4b6e5b386172f4152b49577696639</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>led_index</name>
      <anchorfile>structnon__blocking__blink__state__t.html</anchorfile>
      <anchor>a3d80bb44fac9c31373935f46b262978d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>blink_count</name>
      <anchorfile>structnon__blocking__blink__state__t.html</anchorfile>
      <anchor>a2cf6726c9629b2c9bd5ca4e34c368c01</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>max_blinks</name>
      <anchorfile>structnon__blocking__blink__state__t.html</anchorfile>
      <anchor>aecbf1a19f84e9e158e66165b9eb949cc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>last_toggle_time</name>
      <anchorfile>structnon__blocking__blink__state__t.html</anchorfile>
      <anchor>aa4a0712510b6821b0a1367da1149b74f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>blink_interval_ms</name>
      <anchorfile>structnon__blocking__blink__state__t.html</anchorfile>
      <anchor>a3ff35a22d08f76595b205492b88c0409</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>led_state</name>
      <anchorfile>structnon__blocking__blink__state__t.html</anchorfile>
      <anchor>ac30c937f5ac9092e85e8a313fb94830c</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>rgb_color_t</name>
    <filename>structrgb__color__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>r</name>
      <anchorfile>structrgb__color__t.html</anchorfile>
      <anchor>a43cb2f2dc1e60fa14bf93d90f9d7015e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>g</name>
      <anchorfile>structrgb__color__t.html</anchorfile>
      <anchor>ae73935f5f2386b4c62fd24a8ec6d5512</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>b</name>
      <anchorfile>structrgb__color__t.html</anchorfile>
      <anchor>a6e7156f19f70236f20113f612b7ea1d2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>shared_buffer_t</name>
    <filename>structshared__buffer__t.html</filename>
    <member kind="variable">
      <type>char</type>
      <name>data</name>
      <anchorfile>structshared__buffer__t.html</anchorfile>
      <anchor>a6e6b1fce9ee8c0ae6f0668470107d385</anchor>
      <arglist>[2048]</arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>in_use</name>
      <anchorfile>structshared__buffer__t.html</anchorfile>
      <anchor>ac84993dc918ca7386ae7b700c99039ef</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TaskHandle_t</type>
      <name>owner</name>
      <anchorfile>structshared__buffer__t.html</anchorfile>
      <anchor>aa9d19708eca1a5a3dd0589e3e1f73ad6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>allocated_time</name>
      <anchorfile>structshared__buffer__t.html</anchorfile>
      <anchor>ae40a61faa1d8719d9410054f24306a35</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>file</name>
      <anchorfile>structshared__buffer__t.html</anchorfile>
      <anchor>a903ad299c13da39f391e809d65e0b6a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>line</name>
      <anchorfile>structshared__buffer__t.html</anchorfile>
      <anchor>a00bbd381df7a7778ef9aeaebe0b7d667</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>streaming_output_t</name>
    <filename>structstreaming__output__t.html</filename>
    <member kind="variable">
      <type>stream_type_t</type>
      <name>type</name>
      <anchorfile>structstreaming__output__t.html</anchorfile>
      <anchor>a895a5d5ffe995c94373720aa83bd6f24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>uart_port</name>
      <anchorfile>structstreaming__output__t.html</anchorfile>
      <anchor>a0d62b26ed0234853854e4c89678bdedf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>web_client</name>
      <anchorfile>structstreaming__output__t.html</anchorfile>
      <anchor>a4e343bdd2228d1d6b6ba12be90294f79</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>QueueHandle_t</type>
      <name>queue</name>
      <anchorfile>structstreaming__output__t.html</anchorfile>
      <anchor>aca41fa5e6f2cf2ddf2d0693cdb71cbc9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>auto_flush</name>
      <anchorfile>structstreaming__output__t.html</anchorfile>
      <anchor>a69827b451c4042f64dfccd148efd41b7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stream_line_ending_t</type>
      <name>line_ending</name>
      <anchorfile>structstreaming__output__t.html</anchorfile>
      <anchor>a977a9f506e1525f07917df0db54ddf4f</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>streaming_stats_t</name>
    <filename>structstreaming__stats__t.html</filename>
    <member kind="variable">
      <type>uint32_t</type>
      <name>total_writes</name>
      <anchorfile>structstreaming__stats__t.html</anchorfile>
      <anchor>a3fc6196d74629ffc6f67cbbf37809557</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>total_bytes_written</name>
      <anchorfile>structstreaming__stats__t.html</anchorfile>
      <anchor>ad3c5da4429da544bd592650a4b8cea4a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>write_errors</name>
      <anchorfile>structstreaming__stats__t.html</anchorfile>
      <anchor>acee577c89d193435135d780edd06e52d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>truncated_writes</name>
      <anchorfile>structstreaming__stats__t.html</anchorfile>
      <anchor>ac3d6c57db9ede1dcdd8c63560a655326</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>mutex_timeouts</name>
      <anchorfile>structstreaming__stats__t.html</anchorfile>
      <anchor>a0e9598ecf82660615a899a243c1e191b</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>subtle_animation_state_t</name>
    <filename>structsubtle__animation__state__t.html</filename>
    <member kind="variable">
      <type>bool</type>
      <name>active</name>
      <anchorfile>structsubtle__animation__state__t.html</anchorfile>
      <anchor>acf12e9fc843cf75d5976e43e64c73a95</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>subtle_anim_type_t</type>
      <name>type</name>
      <anchorfile>structsubtle__animation__state__t.html</anchorfile>
      <anchor>a28a89503fb8a1a8db1aaf1cef5feac3c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>frame</name>
      <anchorfile>structsubtle__animation__state__t.html</anchorfile>
      <anchor>abb3851357c569eed9dadb3d1930d62e2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>rgb_color_t</type>
      <name>base_color</name>
      <anchorfile>structsubtle__animation__state__t.html</anchorfile>
      <anchor>a7260489ddfb0f0039463d116b0341250</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>system_config_t</name>
    <filename>structsystem__config__t.html</filename>
    <member kind="variable">
      <type>bool</type>
      <name>verbose_mode</name>
      <anchorfile>structsystem__config__t.html</anchorfile>
      <anchor>af8c07d8eebb3404469c34764d2c3fe74</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>quiet_mode</name>
      <anchorfile>structsystem__config__t.html</anchorfile>
      <anchor>afb203397fdfbc87dd48936ede46fe9ab</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>log_level</name>
      <anchorfile>structsystem__config__t.html</anchorfile>
      <anchor>a5eaad775c73b62e2f1fad961c119f500</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>command_timeout_ms</name>
      <anchorfile>structsystem__config__t.html</anchorfile>
      <anchor>a439d7cee1b183f7a6c09b3e85b2faa59</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>brightness_level</name>
      <anchorfile>structsystem__config__t.html</anchorfile>
      <anchor>a05bbd482d93891e6ed1ec8d1068309d7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>test_suite_t</name>
    <filename>structtest__suite__t.html</filename>
    <member kind="variable">
      <type>char</type>
      <name>name</name>
      <anchorfile>structtest__suite__t.html</anchorfile>
      <anchor>ab0ca04cbd4d16fcc3d594179914ce48c</anchor>
      <arglist>[64]</arglist>
    </member>
    <member kind="variable">
      <type>test_t</type>
      <name>tests</name>
      <anchorfile>structtest__suite__t.html</anchorfile>
      <anchor>aa45aadb094d68d71e4f3b8f5ef2d08c9</anchor>
      <arglist>[20]</arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>test_count</name>
      <anchorfile>structtest__suite__t.html</anchorfile>
      <anchor>a0eb6284b46bb1885d8ddcc33a0b33072</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>passed_count</name>
      <anchorfile>structtest__suite__t.html</anchorfile>
      <anchor>a744bf330f073168fa2af9b1ceddc6354</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>failed_count</name>
      <anchorfile>structtest__suite__t.html</anchorfile>
      <anchor>ada7927aa2f5abc47a2eb6d0ba47a79b8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>skipped_count</name>
      <anchorfile>structtest__suite__t.html</anchorfile>
      <anchor>a381950dd64448257a61ecb54fd923161</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>enabled</name>
      <anchorfile>structtest__suite__t.html</anchorfile>
      <anchor>a9d512ba9e5bd1fb560c6ec96fa6ba528</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>test_t</name>
    <filename>structtest__t.html</filename>
    <member kind="variable">
      <type>char</type>
      <name>name</name>
      <anchorfile>structtest__t.html</anchorfile>
      <anchor>acddfc812ecb222dd97c13ff22f806812</anchor>
      <arglist>[64]</arglist>
    </member>
    <member kind="variable">
      <type>test_result_t</type>
      <name>result</name>
      <anchorfile>structtest__t.html</anchorfile>
      <anchor>a6148a44083a94dd182a6b2310e830b43</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>start_time</name>
      <anchorfile>structtest__t.html</anchorfile>
      <anchor>a9dc39d06407400b05fbfa9c9a3463020</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>duration_ms</name>
      <anchorfile>structtest__t.html</anchorfile>
      <anchor>ae6d710128516d9ac26905194cad3fc82</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>error_message</name>
      <anchorfile>structtest__t.html</anchorfile>
      <anchor>ad3dc2538f803e20c315072d29c0d4ef7</anchor>
      <arglist>[128]</arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>enabled</name>
      <anchorfile>structtest__t.html</anchorfile>
      <anchor>aa31ad7d9aff71a492034f3ff1ee596a8</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>time_control_config_t</name>
    <filename>structtime__control__config__t.html</filename>
    <member kind="variable">
      <type>time_control_type_t</type>
      <name>type</name>
      <anchorfile>structtime__control__config__t.html</anchorfile>
      <anchor>af313100707e76ca6c609540266c9866d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>initial_time_ms</name>
      <anchorfile>structtime__control__config__t.html</anchorfile>
      <anchor>a76ce0ead776d0b8fcd0350151eb96ba4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>increment_ms</name>
      <anchorfile>structtime__control__config__t.html</anchorfile>
      <anchor>a162132d93fca69ac7a11de84a3c933ad</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>name</name>
      <anchorfile>structtime__control__config__t.html</anchorfile>
      <anchor>ac5f05a43dfc9af7383b21d26235c8425</anchor>
      <arglist>[32]</arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>description</name>
      <anchorfile>structtime__control__config__t.html</anchorfile>
      <anchor>a33dadcbfeb5085df5844171bc5bea672</anchor>
      <arglist>[64]</arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_fast</name>
      <anchorfile>structtime__control__config__t.html</anchorfile>
      <anchor>aa9d8d8cc9914cded99bb765fa6e6be3f</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>uart_command_t</name>
    <filename>structuart__command__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchorfile>structuart__command__t.html</anchorfile>
      <anchor>af0ce3b293bd17508b348c57501626039</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>command_handler_t</type>
      <name>handler</name>
      <anchorfile>structuart__command__t.html</anchorfile>
      <anchor>a6fa82e290fa673960dfbda11d905bfd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>description</name>
      <anchorfile>structuart__command__t.html</anchorfile>
      <anchor>a0a77602f1e7d507c3ea44321ae4baf2b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>usage</name>
      <anchorfile>structuart__command__t.html</anchorfile>
      <anchor>af54b176c77095097bdde582f4d1cbc26</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>requires_args</name>
      <anchorfile>structuart__command__t.html</anchorfile>
      <anchor>ad2edd88434fbd2fb4eda438c08e8a953</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>aliases</name>
      <anchorfile>structuart__command__t.html</anchorfile>
      <anchor>a2415139fb44e34d90c07df89e45e9e50</anchor>
      <arglist>[5]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>uart_message_t</name>
    <filename>structuart__message__t.html</filename>
    <member kind="variable">
      <type>uart_msg_type_t</type>
      <name>type</name>
      <anchorfile>structuart__message__t.html</anchorfile>
      <anchor>ade439b530f80cdb4d4617bc50a0bf5a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>add_newline</name>
      <anchorfile>structuart__message__t.html</anchorfile>
      <anchor>a2e34ba0e8d54f4f6fc585b961b3a9422</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>message</name>
      <anchorfile>structuart__message__t.html</anchorfile>
      <anchor>a3385b2208c79250b05ce2699db11ff5d</anchor>
      <arglist>[256]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>wave_animation_state_t</name>
    <filename>structwave__animation__state__t.html</filename>
    <member kind="variable">
      <type>uint8_t</type>
      <name>center_pos</name>
      <anchorfile>structwave__animation__state__t.html</anchorfile>
      <anchor>acc72a17b7ca0887b4acae5c311285340</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>max_radius</name>
      <anchorfile>structwave__animation__state__t.html</anchorfile>
      <anchor>a6a4f1113953d2497b53501e25dfcf5a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>current_radius</name>
      <anchorfile>structwave__animation__state__t.html</anchorfile>
      <anchor>a95ef4cec8f186f8be1d5d22b0c2857fe</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>wave_speed</name>
      <anchorfile>structwave__animation__state__t.html</anchorfile>
      <anchor>a023bf89bd5ddd84d9793e50ddfe57d23</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>active_waves</name>
      <anchorfile>structwave__animation__state__t.html</anchorfile>
      <anchor>a9d860f0d4cf5173b8f9b42f1ec384acd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>wave_t</type>
      <name>waves</name>
      <anchorfile>structwave__animation__state__t.html</anchorfile>
      <anchor>ad00eb787772807623eeb9ec904de2737</anchor>
      <arglist>[5]</arglist>
    </member>
    <member kind="variable">
      <type>uint32_t</type>
      <name>frame</name>
      <anchorfile>structwave__animation__state__t.html</anchorfile>
      <anchor>a14d86f2ce0c27cbaf849141fdaaf53f6</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>wave_t</name>
    <filename>structwave__t.html</filename>
    <member kind="variable">
      <type>float</type>
      <name>radius</name>
      <anchorfile>structwave__t.html</anchorfile>
      <anchor>a1962dccb7b6f3344dd1dc371bef551fb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>active</name>
      <anchorfile>structwave__t.html</anchorfile>
      <anchor>a48a5cf022f2cc66351d4119cabdc3b8d</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="dir">
    <name>animation_task</name>
    <path>components/animation_task/</path>
    <filename>dir_41295a85b1295fc0ebd5beaf3e0347d1.html</filename>
    <dir>include</dir>
    <file>animation_task.c</file>
  </compound>
  <compound kind="dir">
    <name>button_task</name>
    <path>components/button_task/</path>
    <filename>dir_862d50d9bf3f5ceb46867d0cb4c38d54.html</filename>
    <dir>include</dir>
    <file>button_task.c</file>
  </compound>
  <compound kind="dir">
    <name>components</name>
    <path>components/</path>
    <filename>dir_409f97388efe006bc3438b95e9edef48.html</filename>
    <dir>animation_task</dir>
    <dir>button_task</dir>
    <dir>config_manager</dir>
    <dir>enhanced_castling_system</dir>
    <dir>freertos_chess</dir>
    <dir>game_led_animations</dir>
    <dir>game_task</dir>
    <dir>ha_light_task</dir>
    <dir>led_state_manager</dir>
    <dir>led_task</dir>
    <dir>matrix_task</dir>
    <dir>promotion_button_task</dir>
    <dir>reset_button_task</dir>
    <dir>test_task</dir>
    <dir>timer_system</dir>
    <dir>uart_commands_extended</dir>
    <dir>uart_task</dir>
    <dir>unified_animation_manager</dir>
    <dir>visual_error_system</dir>
    <dir>web_server_task</dir>
  </compound>
  <compound kind="dir">
    <name>config_manager</name>
    <path>components/config_manager/</path>
    <filename>dir_8da852bb64bdd94ab698cdb4daee3f25.html</filename>
    <dir>include</dir>
    <file>config_manager.c</file>
  </compound>
  <compound kind="dir">
    <name>enhanced_castling_system</name>
    <path>components/enhanced_castling_system/</path>
    <filename>dir_47913373ed45407138817b1e7d644a5d.html</filename>
    <dir>include</dir>
    <file>enhanced_castling_system.c</file>
  </compound>
  <compound kind="dir">
    <name>freertos_chess</name>
    <path>components/freertos_chess/</path>
    <filename>dir_5c6c08d104c204cc3ac851f4559ad479.html</filename>
    <dir>include</dir>
    <file>freertos_chess.c</file>
    <file>led_mapping.c</file>
    <file>shared_buffer_pool.c</file>
    <file>streaming_output.c</file>
  </compound>
  <compound kind="dir">
    <name>game_led_animations</name>
    <path>components/game_led_animations/</path>
    <filename>dir_1181698f7cc37b6be6aee502a0a15bc1.html</filename>
    <dir>include</dir>
    <file>game_led_animations.c</file>
  </compound>
  <compound kind="dir">
    <name>game_task</name>
    <path>components/game_task/</path>
    <filename>dir_7acbcb872a2ebcf5753943ec89f39513.html</filename>
    <dir>include</dir>
    <file>demo_mode_helpers.c</file>
    <file>game_led_direct.c</file>
    <file>game_task.c</file>
  </compound>
  <compound kind="dir">
    <name>ha_light_task</name>
    <path>components/ha_light_task/</path>
    <filename>dir_b118a6ff7afa3dc10b5f01379f4510ce.html</filename>
    <dir>include</dir>
    <file>ha_light_task.c</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/animation_task/include/</path>
    <filename>dir_d0954016f3b51aa8e12d79ad9b090aaa.html</filename>
    <file>animation_task.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/button_task/include/</path>
    <filename>dir_aa1f7e58bbb569f30127133cb6b3945d.html</filename>
    <file>button_task.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/config_manager/include/</path>
    <filename>dir_0d86bd69b7bd52e460c6d68e5d6c8d5e.html</filename>
    <file>config_manager.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/enhanced_castling_system/include/</path>
    <filename>dir_9cf54d1694e6898b089d09e535c8cac6.html</filename>
    <file>enhanced_castling_system.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/freertos_chess/include/</path>
    <filename>dir_f37fe88dea145212e3c3ec30fa23d01d.html</filename>
    <file>chess_types.h</file>
    <file>freertos_chess.h</file>
    <file>led_mapping.h</file>
    <file>shared_buffer_pool.h</file>
    <file>streaming_output.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/game_led_animations/include/</path>
    <filename>dir_8469098ac891347999a5a46e0fb7673a.html</filename>
    <file>game_led_animations.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/game_task/include/</path>
    <filename>dir_33cc7e91f7e0627bade559bf776868f5.html</filename>
    <file>game_colors.h</file>
    <file>game_led_direct.h</file>
    <file>game_task.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/ha_light_task/include/</path>
    <filename>dir_3267c1d62e884c3e01bf3c26b42e35ae.html</filename>
    <file>ha_light_task.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/led_state_manager/include/</path>
    <filename>dir_5a8f9e0d645bd06dc81b0251036488ca.html</filename>
    <file>led_state_manager.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/led_task/include/</path>
    <filename>dir_32a2a3e5d7c14df46caeb99f197cead9.html</filename>
    <file>led_task.h</file>
    <file>led_task_simple.h</file>
    <file>led_unified_api.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/matrix_task/include/</path>
    <filename>dir_f3f2d4124eab7bbfc28315d5ee96ecb5.html</filename>
    <file>matrix_task.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/promotion_button_task/include/</path>
    <filename>dir_a5c61a8ab78e4dbc07599a87e6ffeb5d.html</filename>
    <file>promotion_button_task.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/reset_button_task/include/</path>
    <filename>dir_fe28bc060c133bc9d041b343b99bee79.html</filename>
    <file>reset_button_task.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/test_task/include/</path>
    <filename>dir_653091839c39176b18308733fb321b48.html</filename>
    <file>test_task.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/timer_system/include/</path>
    <filename>dir_f4061742f0d55e22ad8d9337e92bcefc.html</filename>
    <file>timer_system.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/uart_commands_extended/include/</path>
    <filename>dir_bbecea7143f63eaa9d0d1615abb0cf73.html</filename>
    <file>uart_commands_extended.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/uart_task/include/</path>
    <filename>dir_2a9a7da5e1c6c543d4d8212cf79b68b1.html</filename>
    <file>uart_task.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/unified_animation_manager/include/</path>
    <filename>dir_83b182f0943a51efd30406f771957c42.html</filename>
    <file>unified_animation_manager.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/visual_error_system/include/</path>
    <filename>dir_152d5a1b526a9a9c7cf9ba0ff95f77c6.html</filename>
    <file>visual_error_system.h</file>
  </compound>
  <compound kind="dir">
    <name>include</name>
    <path>components/web_server_task/include/</path>
    <filename>dir_dd43ffc1cdd951e881f480cb706d2add.html</filename>
    <file>web_server_task.h</file>
  </compound>
  <compound kind="dir">
    <name>led_state_manager</name>
    <path>components/led_state_manager/</path>
    <filename>dir_a3cb721c9c1de213cf23ce20ac312914.html</filename>
    <dir>include</dir>
    <file>led_state_manager.c</file>
  </compound>
  <compound kind="dir">
    <name>led_task</name>
    <path>components/led_task/</path>
    <filename>dir_2c8c9ab3b152afb528f35b2c32a919e8.html</filename>
    <dir>include</dir>
    <file>led_task.c</file>
  </compound>
  <compound kind="dir">
    <name>main</name>
    <path>main/</path>
    <filename>dir_5c982d53a68cdbcd421152b4020263a9.html</filename>
    <file>main.c</file>
  </compound>
  <compound kind="dir">
    <name>matrix_task</name>
    <path>components/matrix_task/</path>
    <filename>dir_e77e2685ff47a55cd9b2c8ffde70c90d.html</filename>
    <dir>include</dir>
    <file>matrix_task.c</file>
  </compound>
  <compound kind="dir">
    <name>promotion_button_task</name>
    <path>components/promotion_button_task/</path>
    <filename>dir_ab6b3e4c64325f478abea97ff504e20e.html</filename>
    <dir>include</dir>
    <file>promotion_button_task.c</file>
  </compound>
  <compound kind="dir">
    <name>reset_button_task</name>
    <path>components/reset_button_task/</path>
    <filename>dir_d81a3c6208ed0beed325ac804e7f20d3.html</filename>
    <dir>include</dir>
    <file>reset_button_task.c</file>
  </compound>
  <compound kind="dir">
    <name>test_task</name>
    <path>components/test_task/</path>
    <filename>dir_1991cc6dea0a63cfb4490c20aac875c0.html</filename>
    <dir>include</dir>
    <file>test_task.c</file>
  </compound>
  <compound kind="dir">
    <name>timer_system</name>
    <path>components/timer_system/</path>
    <filename>dir_37ff961cb22580004e48f495a430ad7c.html</filename>
    <dir>include</dir>
    <file>timer_system.c</file>
  </compound>
  <compound kind="dir">
    <name>uart_commands_extended</name>
    <path>components/uart_commands_extended/</path>
    <filename>dir_01cfb48b9a719d4c3c7a5fd96745a96a.html</filename>
    <dir>include</dir>
    <file>uart_commands_extended.c</file>
  </compound>
  <compound kind="dir">
    <name>uart_task</name>
    <path>components/uart_task/</path>
    <filename>dir_d0ae06983db71f379ba075ec013a456f.html</filename>
    <dir>include</dir>
    <file>uart_task.c</file>
  </compound>
  <compound kind="dir">
    <name>unified_animation_manager</name>
    <path>components/unified_animation_manager/</path>
    <filename>dir_823818b8139416439cc2663b18364e63.html</filename>
    <dir>include</dir>
    <file>unified_animation_manager.c</file>
  </compound>
  <compound kind="dir">
    <name>visual_error_system</name>
    <path>components/visual_error_system/</path>
    <filename>dir_27a0c766b97c45b6deeaa7a8c2e2ed8a.html</filename>
    <dir>include</dir>
    <file>visual_error_system.c</file>
  </compound>
  <compound kind="dir">
    <name>web_server_task</name>
    <path>components/web_server_task/</path>
    <filename>dir_17f9138bad1f9d2ff44cddc9da54d444.html</filename>
    <dir>include</dir>
    <file>web_server_task.c</file>
  </compound>
  <compound kind="page">
    <name>index</name>
    <title>CZECHMATE v2.4</title>
    <filename>index.html</filename>
    <docanchor file="index.html" title="CZECHMATE v2.4">md_README</docanchor>
  </compound>
</tagfile>
