#include <stdio.h>
#include <string.h>

#include "core/locale.h"
#include "core/log.h"
#include "translation/translation.h"

static void check_language(const char *name, language_type language)
{
    log_info("Checking language:", name, 0);
    translation_load(language);
}

static const char *find(const translation_string *strings, int num_strings, translation_key key)
{
    for (int i = 0; i < num_strings; i++) {
        if (strings[i].key == key) {
            return strings[i].string;
        }
    }
    return 0;
}

// The status texts of the network game carry their numbers and names as "%d" and "%s" (mp/lockstep.c, T4.11): French
// and English must have the same ones, in the same order
static void placeholders(const char *text, char *out)
{
    for (; *text; text++) {
        if (text[0] == '%') {
            *out++ = text[1];
        }
    }
    *out = 0;
}

static int check_placeholders(const char *what, int first_key, int last_key)
{
    const translation_string *french, *english;
    int num_french, num_english;
    translation_french(&french, &num_french);
    translation_english(&english, &num_english);
    int failures = 0;
    for (int key = first_key; key <= last_key; key++) {
        const char *fr = find(french, num_french, key);
        const char *en = find(english, num_english, key);
        char fr_marks[16], en_marks[16];
        if (!fr || !en || !*fr || !*en) {
            printf("WRONG: %s %d is not translated in French and in English\n", what, key);
            failures++;
            continue;
        }
        placeholders(fr, fr_marks);
        placeholders(en, en_marks);
        if (strcmp(fr_marks, en_marks) != 0) {
            printf("WRONG: %s %d has \"%s\" in French and \"%s\" in English\n", what, key, fr_marks, en_marks);
            failures++;
        }
    }
    return failures;
}

int main(void)
{
    check_language("French", LANGUAGE_FRENCH);
    check_language("German", LANGUAGE_GERMAN);
    check_language("Italian", LANGUAGE_ITALIAN);
    check_language("Spanish", LANGUAGE_SPANISH);
    check_language("Japanese", LANGUAGE_JAPANESE);
    check_language("Korean", LANGUAGE_KOREAN);
    check_language("Polish", LANGUAGE_POLISH);
    check_language("Portuguese", LANGUAGE_PORTUGUESE);
    check_language("Russian", LANGUAGE_RUSSIAN);
    check_language("Swedish", LANGUAGE_SWEDISH);
    check_language("Simplified Chinese", LANGUAGE_SIMPLIFIED_CHINESE);
    check_language("Traditional Chinese", LANGUAGE_TRADITIONAL_CHINESE);
    check_language("Czech", LANGUAGE_CZECH);
    check_language("Greek", LANGUAGE_GREEK);
    int failures = check_placeholders("status", TR_MP_STATUS_WAITING_PLAYERS, TR_MP_STATUS_CONNECTED);
    // the announcements of the war and the offer of peace carry the players as "%d" too (D-092)
    failures += check_placeholders("text of the war", TR_MP_WAR_NEWS_BRUTAL_ON_YOU, TR_MP_MILITARY_PEACE_OFFERED);
    return failures ? 1 : 0;
}