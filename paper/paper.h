#ifndef PAPER_H
#define PAPER_H

#include <list>

namespace RDKit {
    class ROMol;
}

extern "C" float** paper(int gpuID, std::list<RDKit::ROMol*>& molecules);

#endif // PAPER_H
