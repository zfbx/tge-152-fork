//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//
// Arcane-FX
//    Changes:
//        shape-sequence-paths -- allow more sequences before overflowing max packet size.
// Copyright (C) Faust Logic, Inc.
//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//

#include "ts/tsShapeConstruct.h"
#include "console/consoleTypes.h"
#include "core/fileStream.h"
#include "core/bitStream.h"

#define TS_IS_RELATIVE_SEQUENCES 0 
 
IMPLEMENT_CO_DATABLOCK_V1(TSShapeConstructor);

S32 gNumTransforms = 0;
S32 gRotTransforms = 0;
S32 gTransTransforms = 0;

TSShapeConstructor::TSShapeConstructor()
{
   mShape = NULL;
   for (S32 i = 0; i<MaxSequences; i++)
      mSequence[i] = NULL;
}

TSShapeConstructor::~TSShapeConstructor()
{
}

bool TSShapeConstructor::onAdd()
{
   if(!Parent::onAdd())
      return false;
   return true;
}

bool TSShapeConstructor::preload(bool server, char errorBuffer[256])
{
   if(!Parent::preload(server, errorBuffer))
      return false;

   bool error = false;
   hShape = ResourceManager->load(mShape);
   if(!bool(hShape))
      return false;

   if (hShape->getSequencesConstructed())
      return true;

   // We want to write into the resource:
   TSShape * shape = const_cast<TSShape*>((const TSShape*)hShape);

   Stream * f;
   for (S32 i=0; i<MaxSequences; i++)
   {
      if (mSequence[i])
      {
         char fileBuf[256];
         dStrcpy(fileBuf, mSequence[i]);

         // spaces and tabs indicate the end of the file name:
         char * terminate1 = dStrchr(fileBuf,' ');
         char * terminate2 = dStrchr(fileBuf,'\t');
         if (terminate1 && terminate2)
            // select the earliest one:
            *(terminate1<terminate2 ? terminate1 : terminate2) = '\0';
         else if (terminate1 || terminate2)
            // select the non-null one:
            *(terminate1 ? terminate1 : terminate2) = '\0';

         f = ResourceManager->openStream(fileBuf);
         if (!f || f->getStatus() != Stream::Ok)
         {
            dSprintf(errorBuffer, 256, "Missing sequence %s for %s",mSequence[i],mShape);
            error = true;
            continue;
         }
         if (!shape->importSequences(f) || f->getStatus()!=Stream::Ok)
         {
            ResourceManager->closeStream(f);
            dSprintf(errorBuffer, 256, "Load sequence %s failed for %s",mSequence[i],mShape);
            return false;
         }
         ResourceManager->closeStream(f);

         // if there was a space or tab in mSequence[i], then a new name was supplied for it
         // change it here...will still be in fileBuf
         if (terminate1 || terminate2)
         {
            // select the latter one:
            char * nameStart = terminate1<terminate2 ? terminate2 : terminate1;
            do
               nameStart++;
            while ( (*nameStart==' ' || *nameStart=='\t') && *nameStart != '\0' );
            // find the name in the shape (but keep the old one there in case used by something else)
            shape->sequences.last().nameIndex = shape->findName(nameStart);
            if (shape->sequences.last().nameIndex == -1)
            {
               shape->sequences.last().nameIndex = shape->names.size();
               shape->names.increment();
               shape->names.last() = StringTable->insert(nameStart,false);
            }
         }
      }
      else
         break;
   }

   if(!error)
      hShape->setSequencesConstructed(true);
   return !error;
}

// AFX CODE BLOCK (shape-sequence-paths) <<
// These changes make room for more sequences before overflowing the maximum packet
// size. Initially based on a forum post by Chris Calef (and also in the Ragdoll pack),
// packData() and unpackData() have been modified to strip path info from sequences
// located in the same directory as the shape prior to packing the stream. The path info
// is then restored when unpacked. It differs from Calef's solution in that only sequence
// paths that actually share the same path as the shape, are stripped. This means users
// can still specify sequences that are not in the same directory as the shape. 
// (Changes nearly identical to Chris Calef's resource are now incorporated in TGE 1.4.)
//
void TSShapeConstructor::packData(BitStream* stream)
{
   Parent::packData(stream);

   // pack the shape's path
   stream->writeString(mShape);

   // copy everything before last '/' to shape_path
   char shape_path[255];
   U32 path_len = (dStrlen(mShape) - dStrlen(dStrrchr(mShape,'/'))) ;
   dStrncpy(shape_path, mShape, path_len);
   shape_path[path_len] = '\0';

   // count all the sequences
   S32 count = 0;
   for (S32 b=0; b<MaxSequences; b++)
      if (mSequence[b])
         count++;

   // pack the sequence count
   stream->writeInt(count,NumSequenceBits);

   // pack the sequences
   for (S32 i=0; i<MaxSequences; i++)
   {
      if (mSequence[i]) 
      {
         // if it shares the shape's path, pack only the last part
         if (dStrncmp(shape_path, mSequence[i], path_len) == 0)
         {
            const char* subtext = mSequence[i] + path_len;
            stream->writeString(subtext);
         }
         // otherwise, pack the whole path
         else
         {
            stream->writeString(mSequence[i]);
         }
      }
   }
}

void TSShapeConstructor::unpackData(BitStream* stream)
{
   Parent::unpackData(stream);

   // unpack the shape's path
   mShape = stream->readSTString();
   
   // copy everything before last '/' to shape_path
   char shape_path[255];
   U32 path_len = (dStrlen(mShape) - dStrlen(dStrrchr(mShape,'/'))) ;
   dStrncpy(shape_path, mShape, path_len);
   shape_path[path_len] = '\0';

   // unpack the sequences
   char seq_name[255];
   S32 i = 0, count = stream->readInt(NumSequenceBits);
   for (; i<count; i++) 
   {
     stream->readString(seq_name);
     if (seq_name[0] == '/')
     {
        char full_seq_name[255];
        dSprintf(full_seq_name, 255, "%s%s", shape_path, seq_name);
        mSequence[i] = StringTable->insert(full_seq_name);
     }
     else
        mSequence[i] = StringTable->insert(seq_name);
   }

   while (i<MaxSequences)
      mSequence[i++] = NULL;
}
/* ORIGINAL CODE
void TSShapeConstructor::packData(BitStream* stream)
{
  char *subtext;

  Parent::packData(stream);
  stream->writeString(mShape);

  S32 count = 0;
  for (S32 b=0; b<MaxSequences; b++)
     if (mSequence[b])
        count++;
        
  stream->writeInt(count,NumSequenceBits);

  for (S32 i=0; i<MaxSequences; i++)
     if (mSequence[i]) {
        if (TS_IS_RELATIVE_SEQUENCES) 
        {
           subtext = (char*)dStrrchr(mSequence[i],'/');
           stream->writeString(subtext);
        }
        else 
        {
           stream->writeString(mSequence[i]);
        }
     }
}

void TSShapeConstructor::unpackData(BitStream* stream)
{
  S32 strLen,endLen,pathLen;
  char shapePath[90],pathedSeq[90];
  char *subtext;

  Parent::unpackData(stream);
  mShape = stream->readSTString();
  
  if (TS_IS_RELATIVE_SEQUENCES) 
  {
     strLen = (int)dStrlen(mShape);
     subtext = (char*)dStrrchr(mShape,'/');
     endLen = (int)dStrlen(subtext);
     pathLen = (strLen - endLen) ;
     dStrncpy(shapePath,mShape,pathLen);
     shapePath[pathLen] = '\0';
  }

  S32 i = 0, count = stream->readInt(NumSequenceBits);
  for (; i<count; i++) {
     mSequence[i] = stream->readSTString();
     if (TS_IS_RELATIVE_SEQUENCES) {
        dSprintf(pathedSeq,90,"%s%s",shapePath,mSequence[i]);
        mSequence[i] = StringTable->insert(pathedSeq);
     }
  }
  while (i<MaxSequences)
     mSequence[i++] = NULL;
}
 */
// AFX CODE BLOCK (shape-sequence-paths) >>

void TSShapeConstructor::initPersistFields()
{
   Parent::initPersistFields();
   addGroup("Media");	
   addField("baseShape", TypeFilename, Offset(mShape, TSShapeConstructor));
   endGroup("Media");	

   char buf[30];
   if (MaxSequences) addGroup("Sequences");	
   for (S32 i=0; i<MaxSequences; i++)
   {
      dSprintf(buf,sizeof(buf),"sequence%i",i);
      addField(buf, TypeFilename, OffsetNonConst(mSequence[i], TSShapeConstructor));
   }
   if (MaxSequences) endGroup("Sequences");	
}


