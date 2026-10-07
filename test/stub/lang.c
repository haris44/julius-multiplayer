#include "core/encoding.h"
#include "core/lang.h"
#include "graphics/font.h"
#include "translation/translation.h"

static uint8_t EMPTY[] = {0};

static lang_message msg;

int lang_load(int is_editor)
{
    return 1;
}

const uint8_t *lang_get_string(int group, int index)
{
    return EMPTY;
}

const lang_message *lang_get_message(int id)
{
    msg.content.text = EMPTY;
    return &msg;
}

void font_set_encoding(encoding_type encoding)
{}

void translation_load(language_type language)
{}

const uint8_t *translation_for(translation_key key)
{
    return (const uint8_t *) "";
}

// The tests play in French (the language of the project): the texts of the translation table, no other tables
const char *translation_utf8_for(translation_key key)
{
    const translation_string *strings;
    int num_strings;
    translation_french(&strings, &num_strings);
    for (int i = 0; i < num_strings; i++) {
        if (strings[i].key == key) {
            return strings[i].string;
        }
    }
    return "";
}
