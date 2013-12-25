// -*- C++ -*-

/*
  Copyright (c) 2012 , University of Massachusetts Amherst.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
#include "memtrack.h"
#include "sentinelmap.h"

// Check whether an object should be reported or not. Type is to identify whether it is 
// a malloc or free operation. 
// Then we can match to find whether it is good to tell
void memtrack::check(void * start, size_t size, memTrackType type) {
  // For a lot of case, maybe we only update corresponding data structure here. 
  trackObject *object;
      
  //PRINT("_initialized %d\n", _initialized);

  if(!_initialized) {
    return;
  }

  if(_trackMap.find(start, sizeof(start), &object)) {
   // PRINT("objectsize %d, current size %d\n", object->objectSize, size);
    // Now we should verify the size information for buffer overflow and memory leakage.
    // For use-after-free errors, since it is impossible to know actual object size information,
    // then we do not verify its size information.
    if(object->hasUseafterfree() || object->hasOverflow() || (object->hasLeak() && (object->objectSize == size))) {
      // Now we check the type of this object.
      void * callsites[xdefines::CALLSITE_MAXIMUM_LENGTH]; 
      int depth = selfmap::getCallStack((void **)&callsites);

      object->saveCallsite(size, type, depth, (void **)&callsites[0]);
      // Insert or print.
      if(object->hasLeak()) {
        PRINT("\nLeak object at address %p size %ld. Current call stack:\n", object->start, object->objectSize);
        selfmap::getInstance().printCallStack(depth, (void **)&callsites[0]); 
      }
    }
  }

}

faultyObjectType memtrack::getFaultType(void * start, void * faultyaddr) {
	faultyObjectType type = OBJECT_TYPE_NO_ERROR;
  trackObject *object;
  
	// Find corresponding object 
  if(_trackMap.find(start, sizeof(start), &object)) {
		/* Check whether it is an use-after-free error.
		 	 If yes, this object has been released. 
		 	 We do not track those memory operations issued by this library itself,
		 	 if the object is freed, then it must be a use-after-free error.
		*/
		if(object->isFreed()) {
			if(object->isInsideObject(faultyaddr)) {
				type = OBJECT_TYPE_USEAFTERFREE;
			}
		}
		else {
			/*
				Check whether it is an actual overflow. Two conditions should be meet here.
				A. This place is a sentinel.
				B. It is an overflow.
			*/
			if(sentinelmap::getInstance().isOverflow(faultyaddr, object->start, object->currentObjectSize)) {
				type = OBJECT_TYPE_OVERFLOW;
			}	
		}
	}
	else {
		PRINT("start %p is not tracked\n", start);
		assert(0);
	}

	return type;
}

void memtrack::print(void * start, faultyObjectType type) {
  // For a lot of case, maybe we only update corresponding data structure here. 
  trackObject *object;

  assert(_initialized == true); 

  // Find corresponding object 
  if(_trackMap.find(start, sizeof(start), &object)) {
    // Now we should verify the size information for buffer overflow and memory leakage.
    // For use-after-free errors, since it is impossible to know actual object size information,
    // then we do not verify its size information.
    PRINT("Memory allocation call stack for object starting at %p:\n", start);
     
    // Print its allocation stack.     
    selfmap::getInstance().printCallStack(object->allocSite.depth(), object->allocSite.getCallsite()); 
    
    if(type == OBJECT_TYPE_USEAFTERFREE) {
      assert(object->isFreed() == true);
      PRINT("Memory deallocation call stack:\n");
      selfmap::getInstance().printCallStack(object->freeSite.depth(), object->freeSite.getCallsite()); 
    }
  }
}
