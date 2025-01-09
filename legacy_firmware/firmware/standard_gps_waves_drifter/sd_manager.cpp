
#include "sd_manager.h"
#include "params.h"
#include "RTC.h"

extern APM3_RTC myRTC; // Create instance of RTC class

#define SD_CONFIG SdSpiConfig(spiCS1, SHARED_SPI, SD_SCK_MHZ(24), &SPI1)

SdManager sd_manager;

void displayDirectoryContent(File& aDirectory, byte tabulation) 
{
  File file;
  char fileName[20];

  if (!aDirectory.isDir()) return;
  aDirectory.rewind();

  while (file.openNext(&aDirectory, O_READ)) 
  {
    if (!file.isHidden()) {
      file.getName(fileName, sizeof(fileName));
      for (uint8_t i = 0; i < tabulation; i++) Serial.write('\t');
      Serial.print(fileName);

      if (file.isDir()) 
      {
        Serial.println(F("/"));
        displayDirectoryContent(file, tabulation + 1);
      } 
      else 
      {
        Serial.write('\t'); Serial.print((long unsigned int)file.fileSize()); Serial.println(F(" bytes"));
      }
    }
    file.close();
  }
}

void SdManager::end(void)
{
  Serial.println("SdManager::end");
  if (sd_card_open)
  {
    if (file.isOpen())
    { 
      file.close();
      sd.end();
    }
  }
  digitalWrite(spiCS1, HIGH); // Be sure SD is deselected
  sd_card_open = false;
}

void SdManager::start(void)
{
    digitalWrite(spiCS1, HIGH); // Be sure SD is deselected

    //if (sd.begin(spiCS1, SD_SCK_MHZ(24)) == false) //Standard SdFat
    if (sd.begin(SD_CONFIG) == false) 
    {
      Serial.println("SD Card init failed");
    }
    else
    {
      Serial.println("SD Card init ok");
      sd_card_open = true;
      
      cid_t cid;

      sd.card()->readCID(&cid);
      Serial.printf("Manufacturer ID: 0x%02x OEM ID: %d %d product %c%c%c%c%c\r\n", cid.mid, cid.oid[0], cid.oid[1], cid.pnm[0], cid.pnm[0], cid.pnm[1], cid.pnm[2], cid.pnm[3], cid.pnm[4]);
      Serial.printf("Revision: %d %d Serial number : %u Manufacture date : %d/%d\r\n", cid.prvN(), cid.prvM(), cid.psn(), cid.mdtMonth(), cid.mdtYear());

      File dir;
      
      // Open root directory
      if (dir.open("/")) 
      {
        displayDirectoryContent(dir, 0);
      }
    }
}

int SdManager::GetNextIndex(void)
{
  char fname[128];
  int i;

  i = 0;
  sprintf(fname, "LOG-%04d.BIN", i);

  while (sd.exists (fname))
  {
    i++;
    if (i > 10000)
      break;
    sprintf(fname, "LOG-%04d.BIN", i);
  }

  return (i);
}

int SdManager::write(const char name[], void *ptr, uint32_t len)
{
    Serial.printf("SdManager::write %s : %d at 0x%08x\r\n", name, len, ptr);
    if (not(sd_card_open))
    {
      start();
    }
    if (sd_card_open)
    {
      if (not(file.isOpen()))
      {
        int i = GetNextIndex();
        char fname[128];
        sprintf(fname, "LOG-%04d.BIN", i);
        bool op = file.open(fname, O_RDWR | O_APPEND | O_CREAT);
        Serial.printf("File Open %s returned %d\r\n", fname, op);
      }
      if (file.isOpen())
      {
        char ts[32];

        myRTC.getTime();

        sprintf(ts, "%04d/%02d/%02d %02d:%02d:%02d", myRTC.year+2000, myRTC.month, myRTC.dayOfMonth, myRTC.hour, myRTC.minute, myRTC.seconds);

        uint32_t l_in = len + strlen(name) + 1 + strlen(ts) + 1;
        uint32_t l = 0;
        uint32_t *p = &l_in;

        l = file.write(p, sizeof(len));

        l += file.write(ts, strlen(ts)+1);
        l += file.write(name, strlen(name)+1);
        l += file.write(ptr, len);

        Serial.printf("Wrote %d %d\r\n", l_in, l);

        file.sync();
      }
      
    }

    return 0;
}
