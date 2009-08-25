#ifndef	EASTWOOD_PAKFILE_H
#define	EASTWOOD_PAKFILE_H

#include <iostream>
#include <fstream>
#include <vector>

struct PakFileEntry {
    size_t startOffset;
    size_t endOffset;
    std::string fileName;
};

class PakFile
{
    public:
        PakFile(std::istream &stream);
        ~PakFile();

        unsigned char *getFile(std::string fileName, size_t *size);

        inline std::string getFileName(unsigned int index) {
            return _fileEntry[index].fileName;
        };

        inline unsigned int getNumFiles() {
            return _fileEntry.size();
        };

    private:
        void readIndex();

        std::istream &_stream;
        std::vector<PakFileEntry> _fileEntry;
};

#endif // EASTWOOD_PAKFILE_H
// vim:ts=8:sw=4:et
