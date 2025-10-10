#include <gtest/gtest.h>
#include <cstdio>
#include <vector>
#include <string>
#include <memory>

extern "C" {
    #include "json.h"
    #include "bej_encode.h"
    #include "bej_decode.h"
    #include "bej_dictionary.h"
}

/**
 * @brief Универсальная функция теста JSON → BEJ → JSON
 *
 * @param json_path путь к JSON-файлу
 * @param schema_path путь к бинарному словарю схемы
 * @param annot_path путь к бинарному файлу аннотаций (может быть nullptr)
 */
void test_json_bej_roundtrip(const std::string &json_path,
                             const std::string &schema_path,
                             const std::string &annot_path = "")
{
    char cwd[1024];
    const std::string path_curr = getcwd(cwd, sizeof(cwd));

    std::string full_json = path_curr + "/" + json_path;
    std::string full_schema = path_curr + "/" + schema_path;
    std::string full_annot;
    if (!annot_path.empty())
        full_annot = path_curr + "/" + annot_path;

    // Загружаем JSON
    std::unique_ptr<json_value_t, void(*)(json_value_t*)> json_root(
        json_parse_file(full_json.c_str()), json_free
    );
    ASSERT_NE(json_root.get(), nullptr) << "Failed to parse JSON: " << full_json;

    // Загружаем схемы
    std::unique_ptr<bej_dictionary_t, void(*)(bej_dictionary_t*)> schema_dict(
        bej_dictionary_load_map(full_schema.c_str()), bej_dictionary_free
    );
    ASSERT_NE(schema_dict.get(), nullptr) << "Failed to load schema dictionary";

    std::unique_ptr<bej_dictionary_t, void(*)(bej_dictionary_t*)> annot_dict(
        full_annot.empty() ? nullptr : bej_dictionary_load_map(full_annot.c_str()), bej_dictionary_free
    );

    // Кодируем JSON → BEJ в temp файл
    FILE *tmp = tmpfile();
    ASSERT_NE(tmp, nullptr) << "Failed to create temp file";

    int ok = bej_encode_stream(tmp, json_root.get(),
        schema_dict.get(), annot_dict.get());
    ASSERT_NE(ok, 0) << "Encoding to BEJ failed";

    // Читаем результат из файла
    fseek(tmp, 0, SEEK_END);
    long sz = ftell(tmp);
    ASSERT_GT(sz, 0) << "Encoded BEJ is empty";
    rewind(tmp);

    std::vector<uint8_t> bej_data(static_cast<size_t>(sz));
    fread(bej_data.data(), 1, static_cast<size_t>(sz), tmp);
    fclose(tmp);

    // Декодируем BEJ → JSON
    json_value_t *decoded_raw = bej_decode_buffer(bej_data.data(), bej_data.size(), schema_dict.get(), annot_dict.get());
    ASSERT_NE(decoded_raw, nullptr) << "Failed to decode BEJ buffer";

    const std::unique_ptr<json_value_t, void(*)(json_value_t*)> decoded_json(decoded_raw, json_free);

    ASSERT_TRUE(json_compare(json_root.get(), decoded_json.get())) << "Round-trip JSON mismatch";
}

TEST(BejRoundTrip, Example1) {
    test_json_bej_roundtrip(
        "data/example1.json",
        "dictionaries/Memory_v1.bin",
        "dictionaries/annotation.bin"
    );
}

TEST(BejRoundTrip, Example2) {
    test_json_bej_roundtrip(
        "data/example2.json",
        "dictionaries/Memory_v1.bin",
        "dictionaries/annotation.bin"
    );
}

TEST(BejRoundTrip, Example3) {
    test_json_bej_roundtrip(
        "data/example3.json",
        "dictionaries/Memory_v1.bin",
        "dictionaries/annotation.bin"
    );
}
