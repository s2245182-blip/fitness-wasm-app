#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <emscripten/emscripten.h>
#include <pthread.h> 
#include <atomic>    
#include <string.h>
#include <stdio.h>

struct Menu {
    int id;
    int calorie;
    int protein_g;
    int fat_g;
    int carb_g;
    char name[64]; 
};

struct SearchCriteria {
    int min_cal; int max_cal;
    int min_p;   int max_p;
    int min_c;   int max_c;
    int min_f;   int max_f;
};

struct ThreadArgs {
    int start_index;     
    int end_index;       
    SearchCriteria criteria;
};

std::vector<Menu> g_menu_data;
std::atomic<int> g_found_menu_count(0);
int g_found_ids[5];
std::atomic<int> g_found_fill_count(0);

const char* DISH_NAMES[] = {
    "鶏胸肉の低温調理", "ささみの梅肉和え", "鶏もも肉の照り焼き", "親子丼（皮なし鶏肉）", "タンドリーチキン",
    "ローストビーフ", "牛赤身肉のステーキ", "牛しゃぶしゃぶ", "青椒肉絲", "牛丼（つゆ抜き）",
    "焼き鮭", "鯖の塩焼き", "マグロの刺身", "カツオのたたき", "焼きホッケ",
    "豚ヒレ肉のソテー", "生姜焼き（脂身少なめ）", "豚しゃぶサラダ", "回鍋肉", "とんかつ（ヒレ肉）",
    "冷や奴", "納豆キムチ", "厚揚げの煮物", "枝豆", "豆腐ハンバーグ",
    "ブロッコリーの胡麻和え", "ほうれん草のお浸し", "きんぴらごぼう", "ひじきの煮物", "筑前煮",
    "玄米ごはん", "オートミール粥", "全粒粉パスタ", "十割そば", "冷やし中華（蒸し鶏のせ）",
    "シーザーサラダ（グリルチキン）", "海鮮サラダ", "温野菜サラダ", "ポテトサラダ（マヨ控えめ）", "チョレギサラダ",
    "おにぎり（鮭）", "おにぎり（昆布）", "おにぎり（梅）", "おかゆ", "わかめごはん",
    "味噌汁（豆腐とわかめ）", "豚汁（野菜たっぷり）", "ミネストローネ", "クラムチャウダー", "コンソメスープ",
    "プロテインスムージー", "ギリシャヨーグルト", "ゆで卵", "ナッツ（アーモンド）", "バナナ",
    "麻婆豆腐", "エビチリ", "八宝菜", "水餃子", "シュウマイ",
    "カマスの塩焼き", "ブリの照り焼き", "鯛のポワレ", "サンマの塩焼き", "アジの開き",
    "肉じゃが", "だし巻き卵", "茶碗蒸し", "ゴーヤチャンプルー", "かぼちゃの煮付け",
    "キーマカレー（鶏ひき肉）", "グリーンカレー", "ドライカレー", "ハヤシライス", "オムライス",
    "カルボナーラ（豆乳）", "ペペロンチーノ", "ミートソーススパゲッティ", "ナポリタン", "たらこパスタ",
    "焼きそば", "うどん（かけ）", "ほうとう", "そうめん", "にゅうめん",
    "ビビンバ", "クッパ", "冷麺", "サムギョプサル", "タッカルビ",
    "白身魚のフライ", "アジフライ", "イカの塩辛", "タコの酢の物", "あさりの酒蒸し",
    "鶏肉のカシューナッツ炒め", "レバニラ炒め", "もやしナムル", "ジャーマンポテト", "ロールキャベツ"
};

void generate_menu_data() {
    g_menu_data.clear();
    g_menu_data.reserve(1000000); 
    std::srand(100); 
    for (int i = 0; i < 1000000; ++i) { 
        Menu m;
        m.id = i;
        int name_idx = std::rand() % 100;
        strncpy(m.name, DISH_NAMES[name_idx], 63);
        m.protein_g = std::rand() % 60;
        m.carb_g = std::rand() % 100;
        m.fat_g = std::rand() % 40;
        m.calorie = (m.protein_g * 4) + (m.carb_g * 4) + (m.fat_g * 9);
        g_menu_data.push_back(m);
    }
}

void evaluate_menu_suitability(int start_index, int end_index, const SearchCriteria& criteria) {
    for (int i = start_index; i <= end_index; ++i) {
        const Menu& m = g_menu_data[i];
        if (m.calorie >= criteria.min_cal && m.calorie <= criteria.max_cal &&
            m.protein_g >= criteria.min_p && m.protein_g <= criteria.max_p &&
            m.carb_g >= criteria.min_c && m.carb_g <= criteria.max_c &&
            m.fat_g >= criteria.min_f && m.fat_g <= criteria.max_f) {
            
            g_found_menu_count.fetch_add(1);
            int slot = g_found_fill_count.fetch_add(1);
            if (slot < 5) {
                g_found_ids[slot] = i;
            }
        }
    }
}

void* search_thread_func(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    evaluate_menu_suitability(args->start_index, args->end_index, args->criteria);
    return NULL;
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    float perform_multi_thread_search(int num_threads, int min_cal, int max_cal, int min_p, int max_p, int min_c, int max_c, int min_f, int max_f) {
        if (g_menu_data.empty()) { generate_menu_data(); }
        SearchCriteria criteria = {min_cal, max_cal, min_p, max_p, min_c, max_c, min_f, max_f};
        g_found_menu_count = 0;
        g_found_fill_count = 0;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<ThreadArgs> args(num_threads);
        std::vector<pthread_t> threads(num_threads);
        int total_data = g_menu_data.size();
        int chunk_size = total_data / num_threads;

        for (int i = 0; i < num_threads; ++i) {
            int start = i * chunk_size;
            int end = (i == num_threads - 1) ? total_data - 1 : (start + chunk_size - 1);
            args[i] = {start, end, criteria};
            pthread_create(&threads[i], NULL, search_thread_func, &args[i]);
        }
        for (int i = 0; i < num_threads; ++i) { pthread_join(threads[i], NULL); }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<float, std::milli>(end_time - start_time).count();
    }

    EMSCRIPTEN_KEEPALIVE
    void report_results_to_js() {
        int limit = g_found_fill_count.load();
        if (limit > 5) limit = 5;
        for (int i = 0; i < limit; ++i) {
            const Menu& m = g_menu_data[g_found_ids[i]];
            EM_ASM_({
                const name = UTF8ToString($0);
                if (!window.foundResults.some(r => r.name === name)) {
                    window.foundResults.push({ name: name, cal: $1, p: $2, c: $3, f: $4 });
                }
            }, m.name, m.calorie, m.protein_g, m.carb_g, m.fat_g);
        }
    }

    EMSCRIPTEN_KEEPALIVE
    int get_found_menu_count_value() { return g_found_menu_count; }
}