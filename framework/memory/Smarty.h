#pragma once
std::atomic<bool> g_CloseOverlay(false);

static bool is_authorized = false;
static bool collider_state = false;

int select_aimversion = 0;

const char* aimbot_variables[] = {
    "Head",
    "Neck Left",
    "Neck Right",
    "Left Shoulder",
    "Right Shoulder"
};

const char* aimbot_version[] = {
    "Normal",
    "AI"
};


static int scanModeCombo = 0;
const char* scanModes[] = { "Normal", "Bot Ignore" };

static std::atomic<bool> thread_active(false);
uint32_t lastAppliedMatch = 0;

static bool manual_aimbot = false;
static bool auto_aimbot = false;

static bool adv_aimbot_manual = false;
static bool adv_aimbot_auto = false;

static std::atomic<bool> aim_thread_running(false);
static bool aim_applied_match = false;
static bool last_match_state = false;

static int last_mode = -1;
static uint32_t last_applied_match = 0;


bool femaleNearby = false;

// Advance states
int aimbotMatchDelay = 10;

static int last_aim_version = -1;

bool adv_auto_female_detection = false;

static bool adv_enable_aimbot = false;
static bool adv_ignore_bot_enable = false;
static bool adv_ignore_bot_disable = false;

static bool adv_pause_aimbot = false;
static bool adv_resume_aimbot = false;

static bool adv_enable_collider = false;
static bool adv_disable_collider = false;


static bool aimAppliedThisMatch = false;
static bool aimAppliedThisMatch2 = false;

static int applyMode = 0;
static int applyMode2 = 0;

const char* applyModes[] = {
    "Disable",
    "Enable"
};

const char* applyModes2[] = {
    "Disable",
    "Enable"
};


static bool CaptureBypassOn = false;

bool isAimbotActive = false;
bool isAimEnabled = false;

static bool addressesCached = false;


static bool midinject = true;
static bool fastinject = false;
static bool fastinject2 = false;

static bool pie64 = false;
static bool ai = false;

static int selectarchitecture = 0;
static int priority = 1;
static int priority2 = 0;

static int selectai = 0;
static int LegiAim = 0;
static int BypassEmuSelect = 0;


bool aimfcheck;

bool channelTX = false;
bool Applied = false;


namespace var
{
    bool particle1 = true;
    bool discordrpc = true;
    bool headtracking = false;
    bool scopexit = false;
    bool musicplayer = false;
}


static int selectedDetection = 0;   // 0: Manual, 1: Auto
static int selectedTarget = 0;

const char* matchmode[] = {
    "Normal",
    "Auto"
};

const char* auto_stop[] = {
    "Head",
    "Left Neck",
    "Right Neck",
    "Left Shoulder",
    "Right Shoulder"
};

const char* architecture[] = {
    "Normal",
    "AI"
};

const char* GameType[] = {
    "32 Bit",
    "64 Bit"
};

const char* scanner[] = {
    "Normal",
    "Mid",
    "High"
};

const char* scanner2[] = {
    "Normal",
    "High"
};


static bool musicisRunning = false;
static bool isRunning = false;


bool auto_login_success = false;
bool authorizing = false;
bool isProcessFrozen = false;

bool ModeStream = false;
bool alwaysOnTop = false;

bool fixlag = false;
bool trinage = false;
bool trinage2 = false;

int selectAimbotVersion = 0;

const char* Sniper_version[] = {
    "India",
    "Global"
};

int ScanType = 0;   // 0 = Non Bot Ignore, 1 = Bot Ignore


static float text_animation = 0.00f;
static float loader_animation = 0.f;


static bool isRunning3 = true;

static bool hide = true;
static bool show_login = true;

static bool text_animation_hide = true;

static bool penetrate_walls = false;
static bool selfdestruct = false;

static bool showConfirmDialog = false;
static bool resetTab = false;

static bool isRunning2 = true;



static int slider_int[255];

bool checkbox[999] = { false };


static int keybind[99];


static int iTabs = 0;
static int iSubTabs = 0;

static int selected_Detection = 0;
static int selected_target = 0;
static int select_sniper_region = 0;

static int combo = 0;
static int menu_state = 0;
static int dnd_counter = 0;

static int slider_int0 = 0;

static int opticaly = 255;



static int aimbotv18key;
static int dragv18key;

static int saveentitykey;
static int entityneckkey;
static int entityheadkey;
static int entitydragkey;

static int streamermodekey;
static int pintopkey;
static int fakelagkey;


int fakelag = 0;
int pintop = 0;
int streamermode = 0;

int saveentity = 0;
int entityaimbot = 0;
int entityaimbot2 = 0;

int entitysave = 0;
int entityhead = 0;
int entitydrag = 0;

int entitysniperkey = 0;



//--------------------------------------------------


static auto last_color_change = std::chrono::steady_clock::now();

static int auto_color_index = 0;
static int color_change_speed = 500;   // Default 500ms

static bool LineRunning = false;
static bool DotRunning = false;
static bool TriangleRunning = false;


// Key Binds
static int aim_head;
static int aim_neck;
static int aim_drag;

static int fakelag_key;
static int streamer_mode;

bool aimbotex;


bool Chamsesprgbbox;
bool Chamsespbox;
bool connectchams;
bool TWODCHAMS;



static float fakelag2 = 2.0f;



namespace var
{
    bool aimbotex;
    bool Chamsredantena;
    bool Chamsesprgbbox;
    bool Chamsespbox;

    bool chamsmenu;

    bool Chams2D;
    bool chams3D;

    bool TWODCHAMS;
}


namespace esp
{
    bool money = true;
    bool nickname = true;
    bool weapon = true;
    bool zoom = true;

    bool c4 = true;
    bool HP_line = true;
    bool hit = true;
    bool box = true;
    bool bomb = true;

    static float box_color[4] = { 37 / 255.f, 37 / 255.f, 47 / 255.f, 1.f };

    static float nick_color[4] = { 255 / 255.f, 255 / 255.f, 255 / 255.f, 1.f };
    static float money_color[4] = { 255 / 255.f, 255 / 255.f, 255 / 255.f, 1.f };
    static float zoom_color[4] = { 255 / 255.f, 255 / 255.f, 255 / 255.f, 1.f };

    static float c4_color[4] = { 255 / 255.f, 255 / 255.f, 255 / 255.f, 1.f };
    static float bomb_color[4] = { 255 / 255.f, 255 / 255.f, 255 / 255.f, 1.f };

    static float hp_color[4] = { 255 / 255.f, 255 / 255.f, 255 / 255.f, 1.f };
    static float hp_line_color[4] = { 112 / 255.f, 109 / 255.f, 214 / 255.f, 1.f };

    static float weapon_color[4] = { 255 / 255.f, 255 / 255.f, 255 / 255.f, 1.f };
    static float hit_color[4] = { 255 / 255.f, 255 / 255.f, 255 / 255.f, 1.f };

    int hp = 85;
}


int page = 0;

static float tab_alpha = 0.f;
static float tab_add = 0.f;
static int   active_tab = 0;


std::string btn_txt = "Login";


DWORD picker_flags =
ImGuiColorEditFlags_NoSidePreview
| ImGuiColorEditFlags_AlphaBar
| ImGuiColorEditFlags_NoInputs
| ImGuiColorEditFlags_AlphaPreview;


bool beginmark = true;
bool RGBBB = true;
bool done = false;

bool fake_lag;
bool show_menu = true;

static bool streammode = false;

bool authed = false;



#pragma once