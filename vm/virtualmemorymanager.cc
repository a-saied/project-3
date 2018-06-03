/*
 * VirtualMemoryManager implementation
 *
 * Used to facilitate demand paging through providing a means by which page
 * faults can be handled and pages loaded from and stored to disk.
*/

#include <stdlib.h>
#include <machine.h>
#include "virtualmemorymanager.h"
#include "system.h"

VirtualMemoryManager::VirtualMemoryManager()
{
    fileSystem->Create(SWAP_FILENAME, SWAP_SECTOR_SIZE * SWAP_SECTORS);
    swapFile = fileSystem->Open(SWAP_FILENAME);

    swapSectorMap = new BitMap(SWAP_SECTORS);
    physicalMemoryInfo = new FrameInfo[NumPhysPages];
    //swapSpaceInfo = new SwapSectorInfo[SWAP_SECTORS];
    for(int i = 0; i < NumPhysPages; i++){
      physicalMemoryInfo[i].space = NULL;
    }
    nextVictim = 0;
}

VirtualMemoryManager::~VirtualMemoryManager()
{
    fileSystem->Remove(SWAP_FILENAME);
    delete swapFile;
    delete [] physicalMemoryInfo;
    //delete [] swapSpaceInfo;
}

int VirtualMemoryManager::allocSwapSector()
{
    int location = swapSectorMap->Find() * PageSize; // also marks the bit
    return location;
}
/*
SwapSectorInfo * VirtualMemoryManager::getSwapSectorInfo(int index)
{
    return swapSpaceInfo + index;
    
}
*/
void VirtualMemoryManager::writeToSwap(char *page, int pageSize,
                                       int backStoreLoc)
{
    swapFile->WriteAt(page, pageSize, backStoreLoc);
}

/*
 * Page replacement with  the second chance algorithm
 */
void VirtualMemoryManager::swapPageIn(int virtAddr)
{
        TranslationEntry* currPageEntry;
        /*if(nextVictim>= NumPhysPages) {//no more space available
                fprintf(stderr, "Fatal error: No more space available\n");
                exit(1);
                return;
        }
	*/
	
        FrameInfo* physPageInfo = physicalMemoryInfo + nextVictim;
	int i = 0;
        //We assume this page is not occupied by any process space
	//physPageInfo->space = currentThread->space;
	//physPageInfo->pageTableIndex = virtAddr / PageSize;
        //second chance algorithm performed in this while loop. The final product
        //is either the new victim or a NULL pointer
	//fprintf(stderr, "%d\n", i++);
        while(physPageInfo->space != NULL && getPageTableEntry(physPageInfo)->use == true){
            getPageTableEntry(physPageInfo)->use = false;
            nextVictim = nextVictim + 1;
            nextVictim = nextVictim % NumPhysPages;
            physPageInfo = physicalMemoryInfo + nextVictim;
	    //fprintf(stderr, "%d\n", i++);
        }

        if(physPageInfo->space == NULL){
	  *physPageInfo = FrameInfo();
	  physPageInfo->space = currentThread->space;
	  physPageInfo->pageTableIndex = virtAddr / PageSize;
            currPageEntry = getPageTableEntry(physPageInfo);
            currPageEntry->physicalPage = memoryManager->getPage();
            loadPageToCurrVictim(virtAddr);
        }
        else{ //if we have an actual page to replace (not an empty slot)
            TranslationEntry* garb = getPageTableEntry(physPageInfo);
            if(garb->dirty == true){ //check to see if we have to write back to disk (SWAP)
                char* pageToCopy = machine->mainMemory + garb->physicalPage * PageSize;
                writeToSwap(pageToCopy, PageSize, garb->locationOnDisk);
                garb->dirty = false; 
            }
            //start the swap
            physPageInfo->space = currentThread->space; 
            physPageInfo->pageTableIndex = virtAddr / PageSize;
            currPageEntry = getPageTableEntry(physPageInfo);
            currPageEntry->physicalPage = garb->physicalPage;
            loadPageToCurrVictim(virtAddr);
            garb->valid = false;
        }

        //increment nextVictim so we don't always replace same page :)
        nextVictim = nextVictim + 1;
        nextVictim = nextVictim % NumPhysPages;


        // physPageInfo->space = currentThread->space;
        // physPageInfo->pageTableIndex = virtAddr / PageSize;
        // currPageEntry = getPageTableEntry(physPageInfo);
        // currPageEntry->physicalPage = memoryManager->getPage();
        // loadPageToCurrVictim(virtAddr);
        // nextVictim = nextVictim + 1;
}


/*
 * Cleanup the physical memory allocated to a given address space after its 
 * destructor invokes.
*/
void VirtualMemoryManager::releasePages(AddrSpace* space)
{
    for (int i = 0; i < space->getNumPages(); i++)
    {
        TranslationEntry* currPage = space->getPageTableEntry(i);
    //  int swapSpaceIndex = (currPage->locationOnDisk) / PageSize;
 //     SwapSectorInfo * swapPageInfo = swapSpaceInfo + swapSpaceIndex;
//      swapPageInfo->removePage(currPage);
      //swapPageInfo->pageTableEntry = NULL;

        if (currPage->valid == TRUE)
        {
            //int currPID = currPage->space->getPCB()->getPID();
            int currPID = space->getPCB()->getPID();
            DEBUG('v', "E %d: %d\n", currPID, currPage->virtualPage);
            memoryManager->clearPage(currPage->physicalPage);
            physicalMemoryInfo[currPage->physicalPage].space = NULL; 
        }
        swapSectorMap->Clear((currPage->locationOnDisk) / PageSize);
    }
}

/*
 * After selecting a slot of physical memory as a victim and taking care of
 * synchronizing the data if needed, we load the faulting page into memory.
*/
void VirtualMemoryManager::loadPageToCurrVictim(int virtAddr)
{
    int pageTableIndex = virtAddr / PageSize;
    TranslationEntry* page = currentThread->space->getPageTableEntry(pageTableIndex);
    char* physMemLoc = machine->mainMemory + page->physicalPage * PageSize;
    int swapSpaceLoc = page->locationOnDisk;
    swapFile->ReadAt(physMemLoc, PageSize, swapSpaceLoc);

  //  int swapSpaceIndex = swapSpaceLoc / PageSize;
 //   SwapSectorInfo * swapPageInfo = swapSpaceInfo + swapSpaceIndex;
    page->valid = TRUE;
//    swapPageInfo->setValidBit(TRUE);
//    swapPageInfo->setPhysMemPageNum(page->physicalPage);
}

/*
 * Helper function for the second chance page replacement that retrieves the physical page
 * which corresponds to the given physical memory page information that the
 * VirtualMemoryManager maintains.
 * This return page table entry corresponding to a physical page
 */
TranslationEntry* VirtualMemoryManager::getPageTableEntry(FrameInfo * physPageInfo)
{
  //fprintf(stderr, "%d\n", physPageInfo->pageTableIndex);
    TranslationEntry* page = physPageInfo->space->getPageTableEntry(physPageInfo->pageTableIndex);
    return page;
}

void VirtualMemoryManager::copySwapSector(int to, int from)
{
    char sectorBuf[SectorSize];
    swapFile->ReadAt(sectorBuf, SWAP_SECTOR_SIZE, from);
    swapFile->WriteAt(sectorBuf, SWAP_SECTOR_SIZE, to);
}
