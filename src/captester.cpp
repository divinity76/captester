/*
g++ -std=c++17 -Ofast -Wall -Wextra -Wpedantic -Werror -o captester captester.cpp && ./captester test.file
*/
#include <iostream>
#include <string>
#include <cstring>
#include <string_view>
#include <cassert>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <filesystem>

constexpr auto GB_SIZE = 1 * 1024 * 1024 * 1024;
// return seconds since unix epoch as float with microsecond precision
double microtime()
{
    return (double(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count()) / double(1000000));
}

void string_diff_x(std::string_view a, std::string_view b, size_t max_diffs)
{
    if (max_diffs == 0)
    {
        return;
    }
    if (a.size() != b.size())
    {
        std::cout << "size mismatch: " << a.size() << " != " << b.size() << std::endl;
        return;
    }
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (a[i] != b[i])
        {
            std::cout << "diff at " << i << ": " << int(a[i]) << " != " << int(b[i]) << std::endl;
            --max_diffs;
            if (max_diffs == 0)
            {
                return;
            }
        }
    }
}

void write_all(int handle, std::string_view str)
{
    const size_t input_size = str.size();
    size_t written_total = 0;
    while (written_total < input_size)
    {
        const ssize_t written_now = write(handle, str.data(), str.size());
        // todo: unlikely()
        if (written_now < 0)
        {
            // todo: thread-safe version of strerror()?
            const std::string errstr = std::to_string(errno) + ": " + std::string(strerror(errno)) + ": could only write " + std::to_string(written_total) + " of " + std::to_string(input_size) + " bytes";
            std::cerr << errstr << std::endl;
            throw std::runtime_error(errstr);
        }
        if (written_now == 0)
        {
            const std::string errstr = "write() returned 0: could only write " + std::to_string(written_total) + " of " + std::to_string(input_size) + " bytes";
            std::cerr << errstr << std::endl;
            throw std::runtime_error(errstr);
        }
        written_total += written_now;
        str = str.substr(written_now);
    }
    assert(written_total <= input_size && "written_total > input_size");
}
void read_all(const int handle, char *buf, const size_t total_bytes_to_read)
{
    size_t read_total = 0;
    constexpr size_t max_retries = 100;
    size_t retries = 0;
    while (read_total < total_bytes_to_read)
    {
        const ssize_t read_now = read(handle, buf, total_bytes_to_read - read_total);
        if (read_now < 0)
        {
            const std::string errstr = std::to_string(errno) + ": " + std::string(strerror(errno)) + ": could only read " + std::to_string(read_total) + " of " + std::to_string(total_bytes_to_read) + " bytes";
            std::cerr << errstr << std::endl;
            throw std::runtime_error(errstr);
        }
        if (read_now == 0)
        {
            ++retries;
            if (retries > max_retries)
            {
                const std::string errstr = "read() returned 0: could only read " + std::to_string(read_total) + " of " + std::to_string(total_bytes_to_read) + " bytes - retries: " + std::to_string(retries);
                std::cerr << errstr << std::endl;
                throw std::runtime_error(errstr);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        else
        {
            retries = 0;
        }
        read_total += read_now;
        buf += read_now;
    }
    assert(read_total <= total_bytes_to_read && "read_total > total_bytes_to_read");
}
std::string create_gb_string()
{
    std::string str(GB_SIZE, '\0');
    return str;
}
void initialize_gb_string(std::string &str, const uint32_t gb_nr)
{
    str.clear();
    // should i use HETOLE32(gb_nr) here? or accept the fact that the code use a different string depending on the cpu endianess?
    const uint32_t gb_string = gb_nr;
    for (size_t i = 0; i < GB_SIZE; i += sizeof(gb_string))
    {
        str.append(reinterpret_cast<const char *>(&gb_string), sizeof(gb_string));
    }
}
// lseek wrapper throwing runtime_error on error
off_t xlseek(const int fd, const off_t offset, const int whence)
{
    const off_t ret = lseek(fd, offset, whence);
    if (ret == off_t(-1))
    {
        throw std::runtime_error("lseek failed: " + std::to_string(errno) + ": " + std::string(strerror(errno)));
    }
    return ret;
}

void verify_all_gbs(const int handle, std::string &buf1, std::string &buf2)
{
    const off_t original_pos = xlseek(handle, 0, SEEK_CUR);
    const size_t original_pos_as_size_t = original_pos;
    xlseek(handle, 0, SEEK_SET);
    buf2.resize(GB_SIZE);
    for (size_t i = 0; i < original_pos_as_size_t; i += GB_SIZE)
    {
        initialize_gb_string(buf1, i == 0 ? 0 : i / GB_SIZE);
        read_all(handle, (char *)buf2.data(), buf2.size());
        if (buf1 != buf2)
        {
            std::string errstr = "verify_all_gbs failed on GB # " + std::to_string(i == 0 ? 0 : i / GB_SIZE) + ", actual_data != should_be_data";
            std::cerr << errstr << std::endl;
            string_diff_x(buf1, buf2, 10);
            throw std::runtime_error(errstr);
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " /dev/sdX" << std::endl;
        try
        {
            for (const auto &entry : std::filesystem::directory_iterator("/dev/"))
            {
                std::string name = entry.path().filename().string();
                if(name.find("sd") == 0 || name.find("hd") == 0 || name.find("vd") == 0 || name.find("xvd") == 0 || name.find("nvme") == 0)
                {
                    std::cout << "Found device: " << entry.path() << std::endl;
                }
            }
        }
        catch (std::exception &e)
        {
            // ignored
        }
        return 1;
    }
    std::cout << "strace -p " << getpid() << std::endl;
    const int handle = open(argv[1], O_RDWR);
    if (handle < 0)
    {
        std::cerr << "open() failed: " << std::to_string(errno) << ": " << std::string(strerror(errno)) << std::endl;
        return 1;
    }
    std::cout << "open() succeeded" << std::endl;
    std::string gb_str = create_gb_string();
    std::string buf2 = create_gb_string();
    for (size_t gb_no = 0;; ++gb_no)
    {
        double time;
        std::cout << "testing GB #" << gb_no << std::endl;
        time = microtime();
        //std::cout << "initializing GB string.." << std::flush;
        initialize_gb_string(gb_str, gb_no);
        //std::cout << ". done in " << microtime() - time << " seconds" << std::endl;
        std::cout << "writing GB string.." << std::flush;
        time = microtime();
        write_all(handle, gb_str);
        std::cout << ". syncing io cache with disk.." << std::flush;
        const int syncerr = syncfs(handle);
        if (syncerr < 0)
        {
            std::cerr << "syncfs() failed: " << std::to_string(errno) << ": " << std::string(strerror(errno)) << std::endl;
            return 1;
        }
        std::cout << ". done in " << microtime() - time << " seconds" << std::endl;
        std::cout << "verifying all GBs.." << std::flush;
        time = microtime();
        try
        {
            verify_all_gbs(handle, gb_str, buf2);
        }
        catch (std::exception &e)
        {
            std::cerr << e.what() << std::endl;
            size_t real_size_guessed = gb_no;
            std::cout << "\nREAL SIZE OF DISK IS PROBABLY " << real_size_guessed << " GB-ISH!" << std::endl;
            exit(1);
        }
        std::cout << ". done in " << microtime() - time << " seconds" << std::endl;
        std::cout << "verified real size GB: " << gb_no << std::endl;
    }
}
