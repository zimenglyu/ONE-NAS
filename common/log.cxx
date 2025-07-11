#include <cstdio>
using std::fprintf;
using std::printf;
using std::snprintf;
using std::vsnprintf;

// for va_list, va_start
#include <stdarg.h>

#include <iostream>
using std::ofstream;

#include <thread>
using std::thread;
#include <cstring>

#include "arguments.hxx"
#include "files.hxx"
#include "log.hxx"

using std::cerr;
using std::endl;

#include <chrono>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <ios>
#include <iostream>
#include <fstream>
#include <string>

int32_t Log::std_message_level = INFO;
int32_t Log::file_message_level = INFO;
bool Log::write_to_file = false;
int32_t Log::max_header_length = 256;
int32_t Log::max_message_length = 1024;
int32_t Log::process_rank = -1;
int32_t Log::restricted_rank = -1;

string Log::output_directory = "./logs";

map<thread::id, string> Log::log_ids;
map<string, LogFile*> Log::output_files;

shared_mutex Log::log_ids_mutex;

LogFile::LogFile(FILE* _file) {
    file = _file;
}

void Log::register_command_line_arguments() {
    // CommandLine::create_group("Log", "");
    // CommandLine::
}

int8_t Log::parse_level_from_string(string level) {
    if (level.compare("0") == 0 || level.compare("NONE") == 0 || level.compare("none") == 0) {
        return Log::NONE;
    } else if (level.compare("1") == 0 || level.compare("FATAL") == 0 || level.compare("fatal") == 0) {
        return Log::FATAL;
    } else if (level.compare("2") == 0 || level.compare("ERROR") == 0 || level.compare("error") == 0) {
        return Log::ERROR;
    } else if (level.compare("3") == 0 || level.compare("WARNING") == 0 || level.compare("warning") == 0) {
        return Log::WARNING;
    } else if (level.compare("4") == 0 || level.compare("INFO") == 0 || level.compare("info") == 0) {
        return Log::INFO;
    } else if (level.compare("5") == 0 || level.compare("DEBUG") == 0 || level.compare("debug") == 0) {
        return Log::DEBUG;
    } else if (level.compare("6") == 0 || level.compare("TRACE") == 0 || level.compare("trace") == 0) {
        return Log::TRACE;
    } else if (level.compare("7") == 0 || level.compare("ALL") == 0 || level.compare("all") == 0) {
        return Log::ALL;
    } else {
        cerr << "ERROR: specified an incorrect message level for the Log: '" << level << "'" << endl;
        cerr << "Options are:" << endl;
        cerr << "\t0 or NONE or none" << endl;
        cerr << "\t1 or FATAL or fatal" << endl;
        cerr << "\t2 or ERROR or error" << endl;
        cerr << "\t3 or WARNING or warning" << endl;
        cerr << "\t4 or INFO or info" << endl;
        cerr << "\t5 or DEBUG or debug" << endl;
        cerr << "\t6 or TRACE or trace" << endl;
        cerr << "\t7 or ALL or all" << endl;
        exit(1);
    }
}

void Log::initialize(const vector<string>& arguments) {
    // TODO: should read these from the CommandLine (to be created)

    string std_message_level_str, file_message_level_str;

    get_argument(arguments, "--std_message_level", true, std_message_level_str);
    get_argument(arguments, "--file_message_level", true, file_message_level_str);
    get_argument(arguments, "--output_directory", true, output_directory);
    write_to_file = argument_exists(arguments, "--write_to_file");

    std_message_level = parse_level_from_string(std_message_level_str);
    file_message_level = parse_level_from_string(file_message_level_str);

    // cerr << "std_message_level: " << std_message_level << ", file_message_level: " << file_message_level << endl;

    get_argument(arguments, "--max_header_length", false, max_header_length);
    get_argument(arguments, "--max_message_length", false, max_message_length);

    mkpath(output_directory.c_str(), 0777);
}

void Log::set_rank(int32_t _process_rank) {
    process_rank = _process_rank;
}

void Log::restrict_to_rank(int32_t _restricted_rank) {
    restricted_rank = _restricted_rank;
}

void Log::clear_rank_restriction() {
    restricted_rank = -1;
}

void Log::set_id(string human_readable_id) {
    thread::id id = std::this_thread::get_id();

    // cerr << "setting thread id " << id << " to human readable id: '" << human_readable_id << "'" << endl;

    log_ids_mutex.lock();

    log_ids[id] = human_readable_id;

    log_ids_mutex.unlock();
}

void Log::release_id(string human_readable_id) {
    // cerr << "locking thread from human readable id: '" << human_readable_id << "'" << endl;
    log_ids_mutex.lock();
    // cerr << "releasing thread from human readable id: '" << human_readable_id << "'" << endl;

    if (output_files.count(human_readable_id) == 0) {
        // this file was never created and written to

        // cerr << "ERROR: log id '" << human_readable_id << "' was either already released or not previously set!" <<
        // endl; exit(1);
    } else {
        LogFile* log_file = output_files[human_readable_id];
        fflush(log_file->file);
        fclose(log_file->file);

        delete log_file;
        output_files.erase(human_readable_id);
    }

    log_ids_mutex.unlock();
}

void Log::write_message(
    bool print_header, int8_t message_level, const char* message_type, const char* format, va_list arguments
) {
    thread::id id = std::this_thread::get_id();

    if (log_ids.count(id) == 0) {
        cerr << "ERROR: could not write message from thread '" << id
             << "' because it did not have a human readable id assigned (please use the Log::set_id(string) function "
                "before writing to the Log on any thread)."
             << endl;
        cerr << "message:" << endl;
        vprintf(format, arguments);
        cerr << endl;
        exit(1);
    }

    string human_readable_id = log_ids[id];

    // print the message header into a string
    char header_buffer[max_header_length];
    // we only need to print the header for some messages
    if (print_header) {
        // snprintf(header_buffer, max_header_length, "[%-8s %-20s]", message_type, human_readable_id.c_str());
        snprintf(header_buffer, max_header_length, "[%-7s %-21s]", message_type, human_readable_id.c_str());
    }

    // print the actual message contents into a string
    char message_buffer[max_message_length];
    vsnprintf(message_buffer, max_message_length, format, arguments);

    if (std_message_level >= message_level) {
        if (print_header) {
            printf("%s %s", header_buffer, message_buffer);
        } else {
            printf("%s", message_buffer);
        }
    }

    if (write_to_file && file_message_level >= message_level) {
        LogFile* log_file = NULL;

        // check and see if we've already opened a file for this human readable id, if we haven't
        // open a new one for it

        log_ids_mutex.lock();
        if (output_files.count(human_readable_id) == 0) {
            string output_filename = output_directory + "/" + human_readable_id;
            FILE* outfile = fopen(output_filename.c_str(), "w");
            log_file = new LogFile(outfile);
            output_files[human_readable_id] = log_file;
        } else {
            log_file = output_files[human_readable_id];
        }
        log_ids_mutex.unlock();

        // lock this log_file in case multiple threads are trying to write
        // to the same file
        log_file->file_mutex.lock();
        if (print_header) {
            fprintf(log_file->file, "%s %s", header_buffer, message_buffer);
        } else {
            fprintf(log_file->file, "%s", message_buffer);
        }
        fflush(log_file->file);
        log_file->file_mutex.unlock();
    }
}

bool Log::at_level(int8_t level) {
    return level >= std_message_level || level >= file_message_level;
}

void Log::fatal(const char* format, ...) {
    // don't write if this is the wrong process rank
    if (restricted_rank >= 0 && restricted_rank != process_rank) {
        return;
    }

    // not writing this type of message to either std out or a file
    if (std_message_level < FATAL && file_message_level < FATAL) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    write_message(true, FATAL, "FATAL", format, arguments);
}

void Log::error(const char* format, ...) {
    // don't write if this is the wrong process rank
    if (restricted_rank >= 0 && restricted_rank != process_rank) {
        return;
    }

    // not writing this type of message to either std out or a file
    if (std_message_level < ERROR && file_message_level < ERROR) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    write_message(true, ERROR, "ERROR", format, arguments);
}

void Log::warning(const char* format, ...) {
    // don't write if this is the wrong process rank
    if (restricted_rank >= 0 && restricted_rank != process_rank) {
        return;
    }

    // not writing this type of message to either std out or a file
    if (std_message_level < WARNING && file_message_level < WARNING) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    write_message(true, WARNING, "WARNING", format, arguments);
}

void Log::info(const char* format, ...) {
    // don't write if this is the wrong process rank
    if (restricted_rank >= 0 && restricted_rank != process_rank) {
        return;
    }

    // not writing this type of message to either std out or a file
    if (std_message_level < INFO && file_message_level < INFO) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    write_message(true, INFO, "INFO", format, arguments);
}

void Log::debug(const char* format, ...) {
    // don't write if this is the wrong process rank
    if (restricted_rank >= 0 && restricted_rank != process_rank) {
        return;
    }

    // not writing this type of message to either std out or a file
    if (std_message_level < DEBUG && file_message_level < DEBUG) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    write_message(true, DEBUG, "DEBUG", format, arguments);
}

void Log::trace(const char* format, ...) {
    // don't write if this is the wrong process rank
    if (restricted_rank >= 0 && restricted_rank != process_rank) {
        return;
    }

    // not writing this type of message to either std out or a file
    if (std_message_level < TRACE && file_message_level < TRACE) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    write_message(true, TRACE, "TRACE", format, arguments);
}

void Log::fatal_no_header(const char* format, ...) {
    // don't write if this is the wrong process rank
    if (restricted_rank >= 0 && restricted_rank != process_rank) {
        return;
    }

    // not writing this type of message to either std out or a file
    if (std_message_level < FATAL && file_message_level < FATAL) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    write_message(false, FATAL, "FATAL", format, arguments);
}

void Log::error_no_header(const char* format, ...) {
    // don't write if this is the wrong process rank
    if (restricted_rank >= 0 && restricted_rank != process_rank) {
        return;
    }

    // not writing this type of message to either std out or a file
    if (std_message_level < ERROR && file_message_level < ERROR) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    write_message(false, ERROR, "ERROR", format, arguments);
}

void Log::warning_no_header(const char* format, ...) {
    // don't write if this is the wrong process rank
    if (restricted_rank >= 0 && restricted_rank != process_rank) {
        return;
    }

    // not writing this type of message to either std out or a file
    if (std_message_level < WARNING && file_message_level < WARNING) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    write_message(false, WARNING, "WARNING", format, arguments);
}

void Log::info_no_header(const char* format, ...) {
    // don't write if this is the wrong process rank
    if (restricted_rank >= 0 && restricted_rank != process_rank) {
        return;
    }

    // not writing this type of message to either std out or a file
    if (std_message_level < INFO && file_message_level < INFO) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    write_message(false, INFO, "INFO", format, arguments);
}

void Log::debug_no_header(const char* format, ...) {
    // don't write if this is the wrong process rank
    if (restricted_rank >= 0 && restricted_rank != process_rank) {
        return;
    }

    // not writing this type of message to either std out or a file
    if (std_message_level < DEBUG && file_message_level < DEBUG) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    write_message(false, DEBUG, "DEBUG", format, arguments);
}

void Log::trace_no_header(const char* format, ...) {
    // don't write if this is the wrong process rank
    if (restricted_rank >= 0 && restricted_rank != process_rank) {
        return;
    }

    // not writing this type of message to either std out or a file
    if (std_message_level < TRACE && file_message_level < TRACE) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    write_message(false, TRACE, "TRACE", format, arguments);
}

void Log::divider() {
    divider(INFO, DIVIDER_MAJOR, nullptr);
}

bool Log::divider(int8_t level, int8_t divider_type, const char* message) {
    // don't write if this is the wrong process rank
    if (restricted_rank >= 0 && restricted_rank != process_rank) {
        return false;
    }

    // not writing this type of message to either std out or a file
    if (std_message_level < level && file_message_level < level) {
        return false;
    }

    const char* divider_chars;
    int divider_length;
    
    switch (divider_type) {
        case DIVIDER_MAJOR:
            divider_chars = "=";
            divider_length = 80;
            break;
        case DIVIDER_MINOR:
            divider_chars = "-";
            divider_length = 60;
            break;
        case DIVIDER_SUBTLE:
            divider_chars = ".";
            divider_length = 40;
            break;
        default:
            divider_chars = "=";
            divider_length = 80;
            break;
    }
    
    if (message != nullptr && strlen(message) > 0) {
        // Calculate padding for centered message
        int message_len = strlen(message);
        int total_padding = divider_length - message_len - 2; // -2 for spaces around message
        int left_padding = total_padding / 2;
        int right_padding = total_padding - left_padding;
        
        if (left_padding < 0) left_padding = 0;
        if (right_padding < 0) right_padding = 0;
        
        // Print divider with centered message
        string divider_line = "";
        for (int i = 0; i < left_padding; i++) {
            divider_line += divider_chars;
        }
        divider_line += " " + string(message) + " ";
        for (int i = 0; i < right_padding; i++) {
            divider_line += divider_chars;
        }
        divider_line += "\n";
        
        // Use the appropriate logging function based on level
        switch (level) {
            case FATAL:
                fatal_no_header("%s", divider_line.c_str());
                break;
            case ERROR:
                error_no_header("%s", divider_line.c_str());
                break;
            case WARNING:
                warning_no_header("%s", divider_line.c_str());
                break;
            case INFO:
                info_no_header("%s", divider_line.c_str());
                break;
            case DEBUG:
                debug_no_header("%s", divider_line.c_str());
                break;
            case TRACE:
                trace_no_header("%s", divider_line.c_str());
                break;
            default:
                info_no_header("%s", divider_line.c_str());
                break;
        }
    } else {
        // Print simple divider line
        string divider_line = "";
        for (int i = 0; i < divider_length; i++) {
            divider_line += divider_chars;
        }
        divider_line += "\n";
        
        // Use the appropriate logging function based on level
        switch (level) {
            case FATAL:
                fatal_no_header("%s", divider_line.c_str());
                break;
            case ERROR:
                error_no_header("%s", divider_line.c_str());
                break;
            case WARNING:
                warning_no_header("%s", divider_line.c_str());
                break;
            case INFO:
                info_no_header("%s", divider_line.c_str());
                break;
            case DEBUG:
                debug_no_header("%s", divider_line.c_str());
                break;
            case TRACE:
                trace_no_header("%s", divider_line.c_str());
                break;
            default:
                info_no_header("%s", divider_line.c_str());
                break;
        }
    }
    
    return true;
}

void Log::major_divider(int8_t level, const char* message) {
    divider(level, DIVIDER_MAJOR, message);
}

void Log::minor_divider(int8_t level) {
    divider(level, DIVIDER_MINOR, nullptr);
}

long Log::get_memory_usage_kb() {
    // Read memory usage from /proc/self/status on Linux/Unix systems
    std::ifstream file("/proc/self/status");
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            // Extract the memory value (in kB)
            std::string mem_str = line.substr(7); // Skip "VmRSS:\t"
            size_t end_pos = mem_str.find(" kB");
            if (end_pos != std::string::npos) {
                mem_str = mem_str.substr(0, end_pos);
                return std::stol(mem_str);
            }
        }
    }
    
    // Fallback for non-Linux systems - return -1 to indicate unavailable
    return -1;
}

void Log::log_memory_usage(const string& component_name) {
    long memory_kb = get_memory_usage_kb();
    if (memory_kb > 0) {
        Log::info("MEMORY: %s using %ld KB (%.2f MB)\n", 
                 component_name.c_str(), memory_kb, memory_kb / 1024.0);
    } else {
        Log::debug("MEMORY: Unable to get memory usage for %s\n", component_name.c_str());
    }
}

void Log::log_memory_diff(const string& operation, long before_memory_kb) {
    long after_memory_kb = get_memory_usage_kb();
    if (before_memory_kb > 0 && after_memory_kb > 0) {
        long diff_kb = after_memory_kb - before_memory_kb;
        Log::info("MEMORY_DIFF: %s caused %+ld KB (%+.2f MB) change (before: %ld KB, after: %ld KB)\n",
                 operation.c_str(), diff_kb, diff_kb / 1024.0, before_memory_kb, after_memory_kb);
    }
}
