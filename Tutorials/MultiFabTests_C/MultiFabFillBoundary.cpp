// --------------------------------------------------------------------------
// MultiFabReadWrite.cpp
// --------------------------------------------------------------------------
//  this file writes and reads multifabs.
// --------------------------------------------------------------------------
#include <winstd.H>

#include <new>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#ifndef WIN32
#include <unistd.h>
#endif

#include <ParmParse.H>
#include <ParallelDescriptor.H>
#include <Utility.H>
#include <VisMF.H>

#ifdef BL_USE_SETBUF
#define pubsetbuf setbuf
#endif

const int maxGrid(32);
const int pdHi(63);
const int nComp(5);
const int nGhost(4);
const int nFiles(64);


// --------------------------------------------------------------------------
void PrintL0Grdlog(std::ostream &os, const BoxArray &ba, const Array<int> &map) {
  int lev(0);
  int numgrid = ba.size();
  long ncells(0);
  for(int i(0); i < ba.size(); ++i) {
    ncells += ba[i].numPts();
  }
  Real frac(100.0);

  os << "  Level " << lev << "   " << numgrid << " grids  " << ncells << " cells  "
     << frac << " % of domain" << '\n';


  for(int i(0); i < ba.size(); ++i) {
    const Box &b = ba[i];
    os << ' ' << lev << ": " << b << "   ";
    for(int dim(0); dim < BL_SPACEDIM; ++dim) {
      os << b.length(dim) << ' ';
    }
    os << ":: " << map[i] << '\n';
  }
  os << std::endl;

}


// --------------------------------------------------------------------------
int main(int argc, char *argv[]) {

    BoxLib::Initialize(argc,argv);    

    VisMF::SetNOutFiles(nFiles);  // ---- this will enforce the range [1, nprocs]

    // ---- make a box, then a boxarray with maxSize
    Box bDomain(IntVect(0,0,0), IntVect(pdHi,pdHi,pdHi/2));
    BoxArray ba(bDomain);
    ba.maxSize(maxGrid);
    if(ParallelDescriptor::IOProcessor()) {
      std::cout << "ba = " << ba << std::endl;
    }

    // ---- make a multifab, set interior to the index
    // ---- set the ghost regions to -index
    std::string outfile = "MF_Out";
    MultiFab mf(ba, nComp, nGhost);
    for(int i(0); i < mf.nComp(); ++i) {
      mf.setVal(static_cast<Real> (i), i, 1);
      mf.setBndry(static_cast<Real> (-i), i, 1);
    }
    VisMF::Write(mf, outfile);  // ---- write the multifab to MF_Out

    // uncomment the following to write out each fab
    /*
    for(MFIter mfi(mf); mfi.isValid(); ++mfi) {
      const int index(mfi.index());
      FArrayBox &fab = mf[index];
      std::string fname = BoxLib::Concatenate("FAB_", index, 4);
      std::ofstream fabs(fname.c_str());
      fab.writeOn(fabs);
      fabs.close();
    }
    */

    // ---- make a new distribution map
    DistributionMapping dmap(mf.DistributionMap());
    if(ParallelDescriptor::IOProcessor()) {
      std::cout << "dmap = " << dmap << std::endl;
      std::cout << "dmap.size() = " << dmap.size() << std::endl;
    }

    // ------------------------------------------------------------------
    // ----- very important:  here we are copying a procmap,
    // -----                  but if you just make your own Array<int>
    // -----                  it must have an extra value at the end
    // -----                  set to ParallelDescriptor::MyProc()
    // -----                  see DistributionMapping.H
    // ------------------------------------------------------------------
    const Array<int> procMap = dmap.ProcessorMap();
    if(ParallelDescriptor::IOProcessor()) {
      std::cout << "procMap.size() = " << procMap.size() << std::endl;
    }

    // ---- initialize it to (oldmap + 1) % nprocs
    Array<int> newMap(procMap.size());
    for(int i(0); i < procMap.size(); ++i) {
      newMap[i] = (procMap[i] + 1) % ParallelDescriptor::NProcs();
    }
    DistributionMapping newDMap(newMap);
    if(ParallelDescriptor::IOProcessor()) {
      std::cout << "newDMap = " << newDMap << std::endl;
    }

    // ---- make a new multifab with the new map and copy from mf
    MultiFab mfNewMap;
    mfNewMap.define(ba, nComp, nGhost, newDMap, Fab_allocate);


    // ---- this will fill the ghost regions from intersecting fabs
    mf.FillBoundary();

    if(ParallelDescriptor::IOProcessor()) {
      PrintL0Grdlog(std::cout, mf.boxArray(), dmap.ProcessorMap());
    }

    BoxLib::Finalize();
    return 0;
}
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
