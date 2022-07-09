#include <unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h> 
#include <iostream>
#include <stdlib.h> 
#include <string>
#include <vector>
#include "ftd2xx.h"
#include "libft4222.h"
#include "DrvSPIs.h"
#include <Magick++.h>
#include <chrono>

using namespace Magick;
using namespace std;


namespace {
  vector<FT_DEVICE_LIST_INFO_NODE>FT4222DevList;
  FT_HANDLE ftHandle = (FT_HANDLE)NULL;
  const char *usageBanner = "Usage: WEI_SPIrecvImg [Img count]\r\n     Img count=0 for infinite record loop\r\n     Img count=N to record N images\r\n";
} // namespace

int InitFT4222() {
  FT_STATUS ftStatus;
  DWORD numDevs = 0;
  int i, retCode = 0,found4222 = 0;

  do {
    ftStatus = FT_CreateDeviceInfoList(&numDevs);
    if (ftStatus != FT_OK) {
      printf("FT_CreateDeviceInfoList failed (error code %d)\n", (int)ftStatus);
      retCode = -10;
      break;
    }
    if (numDevs == 0) {
      printf("No devices connected.\n");
      retCode = -20;
      break;
    }

    for (DWORD iDev = 0; iDev < numDevs; ++iDev) {
      FT_DEVICE_LIST_INFO_NODE devInfo {0};

      ftStatus = FT_GetDeviceInfoDetail(iDev, &devInfo.Flags, &devInfo.Type, &devInfo.ID, &devInfo.LocId,
                                          devInfo.SerialNumber,
                                          devInfo.Description,
                                          &devInfo.ftHandle);

        if (FT_OK == ftStatus) {
            string desc = devInfo.Description;

            if (desc == "FT4222" || desc == "FT4222 A")
            {
                FT4222DevList.push_back(devInfo);
                found4222 ++;
                //cout<<devInfo.Description<<endl;
            }
        }
    }

    if (found4222 == 0)
        cout<<"No FT4222H detected."<<endl;
  }while(false);

  return retCode;
}

int OpenFT4222() {
  int retCode = 0;
  FT_STATUS ftStatus;
  
  do {
    //use first of FT4222
    if(FT4222DevList.size() == 0) {
      break;
    }
    ftStatus = FT_OpenEx(reinterpret_cast<PVOID>(FT4222DevList[0].LocId), 
                         FT_OPEN_BY_LOCATION, 
                         &ftHandle);
    if (ftStatus != FT_OK) {
        printf("FT_OpenEx failed (error %d)\n", (int)ftStatus);
        break;
    }
    
  }while(false);

  return retCode;  
} 

int storeImage(vector<uint8>&img, string&name ) {
  int fd;
  int nameLnegth = name.length();
  //char nameArr[nameLnegth+1];
  //strcpy(nameArr,name.c_str());
  fd = open (name.c_str(),O_CREAT | O_TRUNC | O_RDWR, 0666) ;
  if(fd == -1) {
    cout<<"unable to open file"<<endl;
    return -1;
  }    
  else {
    if(write(fd, &img[0], sizeof(uint8)*img.size()) <0) {
      cout<<"write file fail"<<endl;
    }
    close(fd);
  }
  cout<<name<<endl;
  return 0;
}
int main(int argc, char *argv[]) {

    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::duration;
    using std::chrono::milliseconds;
    
  //Blob blob;
  uint32 fileLength = 0;
  uint8 buf;
  vector<uint8>fileData;
  vector<uint32> bboxdata;
  string fileName, fullName;
  uint32 fileNo=0;
  long long totalFileNo = 1;
  //vector<uint8> imgData;
  uint8 flag;
  if(argc > 2 || argc == 1)  {
    cout<<usageBanner;  
    return -1;
  }
  else {
   totalFileNo = atoi(argv[1]);
  }
  fileName = "default";

  InitFT4222();
  OpenFT4222();
  
  DrvSpiSInit((PVOID)ftHandle);
  do {
    vector<uint8> imgData;

    try{
     auto t1 = high_resolution_clock::now();
    flag = DRVSpiSRecveData(imgData);
    if(flag == 1) continue;
    uint32 imgdatasize = imgData.size();
    cout << "imgData size: " << imgdatasize << endl;
    if(imgdatasize > 30000){ //error check
    continue;
    }
    // Read image binary data into jpeg format
    Blob blob( &imgData[0], sizeof(uint8)*imgData.size() );
    Image image;
    image.size("640x480");
    image.magick("JPEG");
    image.read(blob);
    
    flag = DRVSpiSRecveData(imgData); //read meta data
    auto t2 = high_resolution_clock::now();
    auto ms_int = duration_cast<milliseconds>(t2 - t1);
    std::cout <<  "time inference" <<ms_int.count() << "ms\n";
    uint32 metadatasize = imgData.size();
    if(metadatasize > 3000){
    	cout << "[-] Error - JPEG Photo has corrupt meta data" << endl;
    	continue;
    }
    if (flag == 1) continue;
    
    // left bitwise shift
    uint32_t count = ((uint32_t)imgData[250]) | (imgData[251] << 8);

    if(imgData[252] == 1 and count >= 1){ //if an object is detected
    	
    	for(int i=0;i<count*68;i=i+68){
    		uint32_t x = ((uint32_t)imgData[256+i]) | (imgData[257+i] << 8) | (imgData[258+i] << 16) | (imgData[259+i] << 24);
        uint32_t y = ((uint32_t)imgData[260+i]) | (imgData[261+i] << 8) | (imgData[262+i] << 16) | (imgData[263+i] << 24);
        uint32_t w = ((uint32_t)imgData[264+i]) | (imgData[265+i] << 8) | (imgData[266+i] << 16) | (imgData[267+i] << 24);
        uint32_t h = ((uint32_t)imgData[268+i]) | (imgData[269+i] << 8) | (imgData[270+i] << 16) | (imgData[271+i] << 24);
        uint32_t class_identifier = ((uint32_t)imgData[272+i]) | (imgData[273+i] << 8) | (imgData[274+i] << 16) | (imgData[275+i] << 24);
        uint32_t score = ((uint32_t)imgData[276+i]);
		
		    bboxdata.push_back(static_cast<unsigned> (x)); //upper left x
	    	bboxdata.push_back(static_cast<unsigned> (y)); //upper left y
	    	bboxdata.push_back(static_cast<unsigned> (w)); // x+w
	    	bboxdata.push_back(static_cast<unsigned> (h)); // y-h
	    	bboxdata.push_back(static_cast<unsigned> (score)); // score
	    	bboxdata.push_back(static_cast<unsigned> (class_identifier)); // class_identifier
    	}
	
    	if(!bboxdata.empty()){
        // Construct drawing list 
        std::vector<Magick::Drawable> drawList;
        std::vector<Magick::Drawable> textlist;
        // Add some drawing options to drawing list
        textlist.push_back(DrawableFont("-misc-fixed-medium-r-normal--20-200-75-75-c-100-iso8859-1"));
        textlist.push_back(DrawableStrokeColor("yellow")); // Outline color
        textlist.push_back(DrawableFillColor("yellow")); // Fill color
        drawList.push_back(DrawableStrokeColor("red")); // Outline color
        drawList.push_back(DrawableStrokeWidth(5)); // Stroke width
        drawList.push_back(DrawableFillColor("none")); // Fill color
        string class_name;
        // Ddit this part accordingly
        for(int j=0;j<count*6;j=j+6){
          if(bboxdata[5+j] == 1){
            class_name = "without_mask";
          }
          else class_name = "with_mask";
          string score_str = "Score: "+ to_string(bboxdata[4+j]) + " Class: " + class_name;
          textlist.push_back(DrawableText(bboxdata[0+j], bboxdata[1+j], score_str));
          drawList.push_back(DrawableRectangle(bboxdata[0+j],bboxdata[1+j],bboxdata[0+j]+bboxdata[2+j],bboxdata[1+j]+bboxdata[3+j]));
          image.draw(drawList);
          image.draw(textlist);
        }
      }
    }
    else{ // Wipe data if no objects
    	bboxdata.clear();
    }
    	

    Image temp_image(image); 
    cout << "\n--------------------------Image Generated!--------------------------\n";
    temp_image.display();
    bboxdata.clear();

    }
    catch(...){
    
    	std::exception_ptr p = std::current_exception();
    	std::clog <<(p ? p.__cxa_exception_type()->name() : "null") << std::endl;
    	cout << "[-] Error - metadata was sent without jpeg image\n";
    }
    
    //fullName="file/"+fileName+to_string(fileNo)+".dat";
    //if(storeImage(imgData,fullName)!=0)
    //  break;
    fileNo++;

    if(totalFileNo!= 0 && fileNo>=totalFileNo)
      break;
  }while(1);
  //cout<<"recv 1 img"<<endl;
  return 0;

    
  
}
