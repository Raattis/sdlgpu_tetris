: " In sh this syntax begins a multiline comment, whereas in batch it's a valid label that gets ignored.
@goto batch_bootstrap_builder "
if false; then */
#error Remember to insert "#if 0" into the compiler input pipe or skip the first 6 lines when compiling this file.
// Notepad++ run command: cmd /c 'cd /d $(CURRENT_DIRECTORY) &amp;&amp; $(FULL_CURRENT_PATH)'
#endif // GOTO_BOOTSTRAP_BUILDER

///////////////////////////////////////////////////////////////////////////////

#ifdef BOOTSTRAP_BUILDER
/*
fi # sh_bootstrap_builder

# Did you know that hashbang doesn't have to be on the first line of a file? Wild, right!
#!/usr/bin/env sh

compiler_executable=gcc
me=`basename "$0"`
no_ext=`echo "$me" | cut -d'.' -f1`
executable="${no_ext}.exe"

echo "static const char* b_source_filename = \"$me\";
#line 1 \"$me\"
#if GOTO_BOOTSTRAP_BUILDER /*" | cat - $me | $compiler_executable -x c -o $executable -DHELLO_WORLD -

compiler_exit_status=$?
if test $compiler_exit_status -ne 0; then echo "Failed to compile $me. Exit code: $compiler_exit_status"; exit $compiler_exit_status; fi

execution_exit_status=$?
if test $execution_exit_status -ne 0; then echo "$executable exited with status $execution_exit_status"; exit $execution_exit_status; fi

# -run -bench -nostdlib -lmsvcrt(?) -nostdinc -Iinclude
exit 0

///////////////////////////////////////////////////////////////////////////////

:batch_bootstrap_builder
@echo off
set compiler_executable=tcc\tcc.exe
set compiler_zip_name=tcc-0.9.27-win64-bin.zip
set download_tcc=n
if not exist %compiler_executable% if not exist %compiler_zip_name% set /P download_tcc="Download Tiny C Compiler? Please, try to avoid unnecessary redownloading. [y/n] "

if not exist %compiler_executable% (
	if not exist %compiler_zip_name% (
		if %download_tcc% == y (
			powershell -Command "Invoke-WebRequest http://download.savannah.gnu.org/releases/tinycc/%compiler_zip_name% -OutFile %compiler_zip_name%"
			if exist %compiler_zip_name% (
				echo Download complete!
			) else (
				echo Failed to download %compiler_zip_name%
			)
		)

		if not exist %compiler_zip_name% (
			echo Download Tiny C Compiler manually from http://download.savannah.gnu.org/releases/tinycc/ and unzip it here.
			pause
			exit /b 1
		)
	)

	if not exist tcc (
		echo Unzipping %compiler_zip_name%
		powershell Expand-Archive %compiler_zip_name% -DestinationPath .

		if exist %compiler_executable% (
			echo It seems the %compiler_zip_name% contained the %compiler_executable% directly. Thats cool.
		) else if not exist tcc (
			echo Unzipping %compiler_zip_name% did not yield the expected "tcc" folder.
			echo Move the contents of the archive here manually so that tcc.exe is in the same folder as %~n0%~x0.
			pause
			exit /b 1
		)
	)

	echo Tiny C Compiler Acquired!
)

rem Add tcc subfolder to path to enable finding the lib and dll files
set PATH=%PATH%;tcc

(
	echo static const char* b_source_filename = "%~n0%~x0";
	echo #line 0 "%~n0%~x0"
	echo #if GOTO_BOOTSTRAP_BUILDER
	type %~n0%~x0
) | %compiler_executable% -run -DSOURCE -nostdinc -lmsvcrt -lkernel32 -luser32 -lgdi32 -Itcc/include -Itcc/include/winapi -Itcc/libtcc -Ltcc/libtcc -llibtcc -bench -
rem ) | %compiler_executable% -o%~n0.exe -DSOURCE -bench -Ilibtcc -lmsvcrt -lkernel32 -luser32 -lgdi32 -Llibtcc -llibtcc -


echo.

if %errorlevel% == 0 (
	echo Finished without errors!
) else (
	if %errorlevel% == -1073740940 (
		echo %errorlevel% - Critical error detected C0000374
	) else (
		if %errorlevel% == -1073741819 (
			echo %errorlevel% - Access right violation C0000005
		) else (
			if %errorlevel% == -1073740771 (
				echo %errorlevel% - STATUS_FATAL_USER_CALLBACK_EXCEPTION C000041D
			) else (
				echo Finished with error %errorlevel%
			)
		)
	)
)

REM %~n0.exe

:end
exit /b errorlevel

*/
#endif // BOOTSTRAP_BUILDER

///////////////////////////////////////////////////////////////////////////////

#ifdef HELLO_WORLD

#include <stdio.h>

int main()
{
	printf("Hello, World!\n");
	return 0;
}

#endif // HELLO_WORLD

///////////////////////////////////////////////////////////////////////////////

#ifdef SOURCE
#define SEGMENT_NAME "SOURCE"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>

#include <libtcc.h>

enum { TRACE=1, TRACE_VERBOSE=0 };
enum { TRACE_INPUT=0&&TRACE, TRACE_TICKS=0&&TRACE, TRACE_PAINT=0&&TRACE };

#define trace_printf(...) do { if (TRACE) printf(__VA_ARGS__); } while(0)
#define trace_func() trace_printf("%s, ", __FUNCTION__)
#define trace_func_end() trace_printf("%s end, ", __FUNCTION__)

#define verbose_printf(...) do { if (TRACE_VERBOSE) printf(__VA_ARGS__); } while(0)
#define verbose_func() verbose_printf("%s, ", __FUNCTION__)
#define verbose_func_end() verbose_printf("%s end, ", __FUNCTION__)

#define FATAL(x, ...) do { if (x) break; fprintf(stderr, "%s:%d: (" SEGMENT_NAME "/%s) FATAL: ", __FILE__, __LINE__, __FUNCTION__); fprintf(stderr, __VA_ARGS__ ); fprintf(stderr, "\n(%s)\n", #x); int system(const char*); system("pause"); void exit(int); exit(-54746); } while(0)

#define input_printf(...) do { if (TRACE_INPUT) printf(__VA_ARGS__); } while(0)
#define paint_printf(...) do { if (TRACE_PAINT) printf(__VA_ARGS__); } while(0)
#define tick_printf(...) do { if (TRACE_TICKS) printf(__VA_ARGS__); } while(0)

signed long long microseconds(void)
{
	clock_t c = clock();
	return ((signed long long)c) / (CLOCKS_PER_SEC / 1000000ull);
}

typedef struct stat file_timestamp;

void get_file_timestamp(file_timestamp* stamp, const char* file)
{
	stat(file, stamp);
}

int cmp_and_swap_timestamps(file_timestamp* stamp1, const char* file)
{
	file_timestamp stamp2;
	get_file_timestamp(&stamp2, file);

	if (stamp1->st_mtime == stamp2.st_mtime)
		return 0;

	if (stamp1->st_mtime < stamp2.st_mtime)
	{
		*stamp1 = stamp2;
		return -1;
	}

	return 1;
}

size_t scan_includes(const char* source_file, char** files_to_watch, size_t files_to_watch_count, size_t written)
{
	trace_printf("scan_includes('%s', %lld)\n", source_file, written);

	char buffer[1024] = {0};

	size_t first_written = written;
	FILE* infile = fopen(source_file, "r");
	while (fgets(buffer, sizeof(buffer), infile))
	{
		if (strstr(buffer, "#include \"") == 0)
			continue;

		char* begin = buffer + strlen("#include \"");
		char* end = begin;
		while(end < buffer + files_to_watch_count && *end != '"' && *end != 0)
			end += 1;

		int found = 0;
		for (size_t i = 0; i < written; i++)
		{
			if (strncmp(files_to_watch[i], begin, end - begin) == 0)
			{
				found = 1;
				break;
			}
		}

		if (found)
			continue;

		char* existing_file = files_to_watch[written];
		if (existing_file == 0 || strncmp(existing_file, begin, end - begin) != 0)
		{
			extern void free(void*);
			extern void* malloc(size_t);
			if (existing_file != 0)
				free(existing_file);

			size_t length = end - begin;
			existing_file = (char*)malloc(length);
			strncpy(existing_file, begin, length);
			existing_file[length] = 0;

			printf("Watching '%s' for changes.\n", existing_file);

			files_to_watch[written] = existing_file;
		}
		written += 1;
	}
	fclose(infile);

	for (size_t i = first_written, end = written; i < end; ++i)
	{
		written = scan_includes(files_to_watch[i], files_to_watch, files_to_watch_count, written);
	}

	return written;
}

size_t find_corresponding_source_files(const char** includes, size_t includes_count, char** sources, size_t sources_count, size_t written_sources)
{
	trace_printf("find_corresponding_source_files(%lld, %lld)\n", includes_count, written_sources);

	char buffer[1024] = {0};
	for (int i = 0; i < includes_count && written_sources < sources_count; ++i)
	{
		printf("checking '%s'\n", includes[i]);
		strcpy(buffer, includes[i]);
		char* ext = strstr(buffer, ".h");
		if (!ext)
			continue;

		ext[1] = 'c';

		char* existing_file = sources[written_sources];
		if (existing_file != 0 && strcmp(existing_file, buffer) == 0)
		{
			written_sources += 1;
			continue;
		}

		struct stat dummy;
		if (stat(buffer, &dummy) == 0)
		{
			extern void free(void*);
			extern void* malloc(size_t);
			if (existing_file != 0)
				free(existing_file);

			existing_file = (char*)malloc(strlen(buffer));
			strcpy(existing_file, buffer);

			sources[written_sources] = existing_file;
			written_sources += 1;
		}
	}

	return written_sources;
}

struct headers_and_sources {
	char* headers[256];
	char* sources[256];
	size_t sources_count;
	size_t headers_count;
};

void get_headers_and_sources(const char* main_source_file, struct headers_and_sources* headers_and_sources)
{
	size_t headers_buffer_size = sizeof(headers_and_sources->headers) / sizeof(headers_and_sources->headers[0]);
	size_t sources_buffer_size = sizeof(headers_and_sources->sources) / sizeof(headers_and_sources->sources[0]);
	void* malloc(size_t);
	char* main_source_file_copy = malloc(strlen(main_source_file) + 1);
	strcpy(main_source_file_copy, main_source_file);
	headers_and_sources->sources[0] = main_source_file_copy;
	headers_and_sources->sources_count = 1;
	headers_and_sources->headers_count = 0;
	for (size_t i = 0; i < headers_and_sources->sources_count; ++i)
	{
		const char* source = headers_and_sources->sources[i];
		trace_printf("Scanning '%s'\n", source);

		size_t prev_headers_count = headers_and_sources->headers_count;

		headers_and_sources->headers_count
			= scan_includes(
				source,
				headers_and_sources->headers,
				headers_buffer_size,
				headers_and_sources->headers_count);

		headers_and_sources->sources_count
			= find_corresponding_source_files(
				headers_and_sources->headers + prev_headers_count,
				headers_and_sources->headers_count - prev_headers_count,
				headers_and_sources->sources,
				sources_buffer_size,
				headers_and_sources->sources_count);
	}
}

int get_any_newer_file_timestamp(file_timestamp* stamp, struct headers_and_sources* headers_and_sources)
{
	int found = 0;
	for (size_t i = 0; i < headers_and_sources->sources_count; ++i)
	{
		if (cmp_and_swap_timestamps(stamp, headers_and_sources->sources[i]) < 0)
		{
			printf("Timestamp of '%s' was newer than previous timestamp\n", headers_and_sources->sources[i]);
			found = 1;
		}
	}

	for (size_t i = 0; i < headers_and_sources->headers_count; ++i)
	{
		if (cmp_and_swap_timestamps(stamp, headers_and_sources->headers[i]) < 0)
		{
			printf("Timestamp of '%s' was newer than previous timestamp\n", headers_and_sources->sources[i]);
			found = 1;
		}
	}

	return found;
}

#define verbose_print_bytes(...) do { if (TRACE_VERBOSE) printf(__VA_ARGS__); } while(0)
void print_bytes(const char* label, void* bytes, size_t length)
{
	if (!TRACE)
		return;

	printf("\n%s [0x%llX..0x%llX]:\n", label, bytes, bytes + length);
	for (int i = 0; i < length; ++i)
	{
		printf("%02X ", ((unsigned char*)bytes)[i]);
		if (((i + 1) % 32) == 0)
			printf("\n");
	}
	printf("\n");
}

#if defined(_WIN32)
#include <windows.h>
void recurse_dir(const char *dir_path) {
    WIN32_FIND_DATA findFileData;
    HANDLE hFind;

    // Construct the search pattern
    char search_pattern[MAX_PATH];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*", dir_path);

    hFind = FindFirstFile(search_pattern, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        perror("FindFirstFile");
        return;
    }

    do {
        // Skip "." and ".."
        if (strcmp(findFileData.cFileName, ".") == 0 || strcmp(findFileData.cFileName, "..") == 0) {
            continue;
        }

        // Construct the full path
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, findFileData.cFileName);

        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            printf("Directory: %s\n", full_path);
            recurse_dir(full_path); // Recurse into the directory
        } else {
            printf("File: %s\n", full_path); // Handle files
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    FindClose(hFind);
}
#else
void recurse_dir(const char *dir_path) {
    struct dirent *entry;
    DIR *dp = opendir(dir_path);

    if (!dp) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dp)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct the full path
        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        // Check if it's a directory
        struct stat statbuf;
        if (stat(full_path, &statbuf) == 0) {
            if (S_ISDIR(statbuf.st_mode)) {
                printf("Directory: %s\n", full_path);
                recurse_dir(full_path); // Recurse into the directory
            } else {
                printf("File: %s\n", full_path); // Handle files
            }
        }
    }

    closedir(dp);
}
#endif

void compile_sdl_static_lib(void)
{
	recurse_dir("sdl-src");
	clock_t c = clock();

	printf("Compiling SDL!\n");

	TCCState* s = tcc_new();
	FATAL(s, "Could not create tcc state\n");

	tcc_set_output_type(s, TCC_OUTPUT_OBJ);
	tcc_set_options(s, "-DWIN32 -D_WINDOWS -DUSING_GENERATED_CONFIG_H -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS -DSDL_STATIC_LIB -DSDL_BUILD_MAJOR_VERSION=3 -DSDL_BUILD_MINOR_VERSION=1 -DSDL_BUILD_MICRO_VERSION=3 -DCMAKE_INTDIR=\"Debug\" -DWINBOOL=\"char\"");

	tcc_add_include_path(s, "sdl-src/src/video/khronos");
	tcc_add_include_path(s, "sdl-build/include-config-debug/build_config");
	tcc_add_include_path(s, "sdl-build/include");
	tcc_add_include_path(s, "sdl-src/include");
	tcc_add_include_path(s, "sdl-src/src");
	tcc_add_include_path(s, "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.22621.0\\shared"); // For mmreg.h
	tcc_add_include_path(s, "tcc/include");
	tcc_add_include_path(s, "tcc/include/winapi");

	tcc_add_library_path(s, "tcc/lib");

	extern int tcc_add_library_err(TCCState *s, const char *f);
	tcc_add_library_err(s, "msvcrt");
	tcc_add_library_err(s, "kernel32");
	tcc_add_library_err(s, "user32");
	
	tcc_set_options(s, "-vv");

	const char* filename = "sdl-src/src/SDL.c";
	int add_file_result = tcc_add_file(s, filename);
	FATAL(-1 != add_file_result, "Couldn't add '%s'.", filename);
	
	
	char target_path[4096];
	snprintf(target_path, sizeof(target_path), "artifacts/%s.o", filename);
	int compilation_result = tcc_output_file(s, target_path);
	FATAL(-1 != compilation_result, "Compiling '%s' failed.", filename);

	clock_t milliseconds = (clock() - c) / (CLOCKS_PER_SEC / 1000ull);
	printf("Compilation took %lld.%03lld seconds.\n", milliseconds/1000ull, milliseconds%1000ull);

	if (s)
		tcc_delete(s);
}

void run(void)
{
	compile_sdl_static_lib();
	//compile_main_c();

	verbose_func_end();
}

LONG exception_handler(LPEXCEPTION_POINTERS p)
{
	FATAL(0, "Exception!!!\n");
	return EXCEPTION_EXECUTE_HANDLER;
}

void _start()
{
	trace_printf("_start()\n");

	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)&exception_handler);

	run();

	trace_printf("\nBye!\n");

	exit(0);
}

void _runmain() { _start(); }

#endif // SOURCE
