#pragma once

#include <fstream>

// read a file in and return it's contents as a std::vector of chars (byte array internally)
static std::vector<char> readFile(const std::string& filename) {
    // create stream
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    // check opening worked
    if (!file.is_open()) {
        // throw error if not
        throw std::runtime_error("failed to open file: " + filename + "!");
    }

    // get filesize
    size_t fileSize = (size_t)file.tellg();
    // create buffer of adaquate size
    std::vector<char> buffer(fileSize);

    // returnt o start of stream
    file.seekg(0);
    // read data into vector's internal array
    file.read(buffer.data(), fileSize);

    // close file
    file.close();

    //return data
    return buffer;
}