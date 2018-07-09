#include <stdext/file.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include <cstdint>
#include <cstdlib>
#include <cwchar>


class usage_error : public std::runtime_error
{
	using runtime_error::runtime_error;
};

static void show_usage(const wchar_t* invocation);

static void extract_all(stdext::file_input_stream& input_file, const wchar_t* output_path);
static void extract_one(stdext::file_input_stream& input_file, unsigned index, const wchar_t* output_path);

static std::string to_mbstring(const wchar_t* str);

int wmain(int argc, wchar_t* argv[])
{
	try
	{
		const wchar_t* input_path = nullptr;
		const wchar_t* output_path = nullptr;
		auto index = unsigned(-1);

		if (argc == 1)
		{
			show_usage(argv[0]);
			return EXIT_SUCCESS;
		}

		for (int n = 1; n < argc; ++n)
		{
			if (argv[n][0] == L'-')
			{
				if (wcscmp(argv[n], L"-extract") == 0)
				{
					if (++n != argc)
					{
						wchar_t* endp;
						index = unsigned(wcstoul(argv[n], &endp, 10));
						if (*endp != L'\0')
							throw usage_error("Bad resource index: " + to_mbstring(argv[n]));
					}
				}
                else if (wcscmp(argv[n], L"-extract-all") == 0)
                {
                    index = unsigned(-2);
                }
				else if (wcscmp(argv[n], L"-o") == 0)
				{
					if (++n == argc)
						throw usage_error("Missing output path");
					output_path = argv[n];
				}
			}
			else
			{
				if (input_path != nullptr)
					throw usage_error("Unrecognized argument: " + to_mbstring(argv[n]));
				input_path = argv[n];
			}
		}

		if (input_path == nullptr)
			throw usage_error("No input path specified");

        if (index == unsigned(-1))
            throw usage_error("No command option specified");

        stdext::file_input_stream input_file(input_path);
        if (index == unsigned(-2))
            extract_all(input_file, output_path);
        else
    		extract_one(input_file, index, output_path);

        return EXIT_SUCCESS;
    }
	catch (const usage_error& e)
	{
        std::cerr << "Error: " << e.what() << '\n';
		show_usage(argv[0]);
	}
	catch (const std::exception& e)
	{
        std::cerr << "Error: " << e.what() << '\n';
	}
	catch (...)
	{
        std::cerr << "Unknown error" << '\n';
	}

	return EXIT_FAILURE;
}

void show_usage(const wchar_t* invocation)
{
    std::wcout << L"Usage: " << invocation << " -o <output_path> -extract <resource_index> <input_path>\n"
                  L"       " << invocation << " -o <output_path> -extract-all <input_path>\n";
}

void extract_all(stdext::file_input_stream& input_file, const wchar_t* output_path)
{
    auto file_size = input_file.read<uint32_t>();
    auto first_resource_offset = input_file.read<uint32_t>() & 0x00FFFFFF;
    auto resource_count = (first_resource_offset - 4) / 4;

    auto dir = std::filesystem::current_path();
    if (output_path == nullptr)
        output_path = dir.c_str();

    std::filesystem::create_directories(output_path);
    for (uint32_t n = 0; n < resource_count; ++n)
    {
        input_file.set_position(0);
        extract_one(input_file, n, (std::filesystem::path(output_path) /= std::to_wstring(n)).c_str());
    }
}

void extract_one(stdext::file_input_stream& input_file, unsigned index, const wchar_t* output_path)
{
    auto descriptor_offset = 4 + (4 * index);
    auto file_size = input_file.read<uint32_t>();
    auto first_resource_offset = input_file.read<uint32_t>() & 0x00FFFFFF;
    if (first_resource_offset <= descriptor_offset)
        throw std::range_error("Resource index " + std::to_string(index) + " out of range");

    input_file.set_position(descriptor_offset);
    auto resource_offset = input_file.read<uint32_t>() & 0x00FFFFFF;
    auto resource_size = input_file.position() == first_resource_offset
        ? file_size - resource_offset
        : (input_file.read<uint32_t>() & 0x00FFFFFF) - resource_offset;

    input_file.set_position(resource_offset);

    uint8_t buf[0x1000];
    stdext::file_output_stream output_file(output_path);
    while (resource_size != 0)
    {
        auto bytes = input_file.read(buf, std::min(size_t(resource_size), stdext::lengthof(buf)));
        output_file.write(buf, bytes);
        resource_size -= bytes;
    }
}

std::string to_mbstring(const wchar_t* str)
{
    std::mbstate_t state = { };
    auto length = std::wcsrtombs(nullptr, &str, 0, &state);
    if (length == size_t(-1))
        throw std::runtime_error("Unicode error");

    std::string result(length, '\0');
    std::wcsrtombs(result.data(), &str, length, &state);
    return result;
}
