#include<cv.h>
#include<iostream>
#include<opencv/highgui.h>
#include<opencv2/face.hpp>
#include<opencv2/opencv.hpp>
#include<opencv2/face/bif.hpp>
#include<opencv2/face/predict_collector.hpp>
#include<opencv2/face/facerec.hpp>
#include<opencv2/objdetect.hpp>

#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/sem.h>
#include<sys/shm.h>
#include<stdlib.h>
#include<errno.h>
#include<sys/wait.h>
#include<string.h>

using namespace cv;
using namespace std;
using namespace face;
string cascadeName;
string nestedCascadeName;

//全局变量
int IsFace = 0;
int FaceSentence = 0;
static Mat norm_0_255(InputArray _src) {
    Mat src = _src.getMat();
    // 创建和返回一个归一化后的图像矩阵:
    Mat dst;
    switch(src.channels()) {
    case 1:
        cv::normalize(_src, dst, 0,255, NORM_MINMAX, CV_8UC1);
        break;
    case 3:
        cv::normalize(_src, dst, 0,255, NORM_MINMAX, CV_8UC3);
        break;
    default:
        src.copyTo(dst);
        break;
    }
    return dst;
}
//全局变量
typedef struct BufferArea{
    unsigned char num[5];
}AREA;
union semun{
    short val;
    struct semid_ds *buf;
    unsigned short *arry;
};
int SemKey = 0xAE12;
int ShmKey = 0xAE86;
struct sembuf SopsP;
struct sembuf SopsV;
int SemID;
int ShmID;
AREA *ShmAddr = NULL;
union semun sem_union;

void SemInit(){
    SemID = semget(SemKey,3,0);
    if(SemID == -1){
        printf("创建信号量失败\n");
        exit(0);
    }
    printf("SemID:%d\n",SemID);
    SopsP.sem_num = 0;
    SopsP.sem_op = -1;
    SopsP.sem_flg = SEM_UNDO;
    SopsV.sem_num = 0;
    SopsV.sem_op = 1;
    SopsV.sem_flg = SEM_UNDO;
    sem_union.val = 1;
}
void ShmInit(){
    ShmID = shmget(ShmKey,sizeof(struct BufferArea),0);
    if(ShmID == -1){
        printf("创建共享内存失败\n");
        exit(0);
    }
    printf("ShmID:%d\n",ShmID);
    ShmAddr = (AREA*)shmat(ShmID,NULL,0);
    if(ShmAddr == (void *)-1){
        printf("共享内存连接失败\n");
        semctl(SemID,0,IPC_RMID,sem_union); //删除信号量
        exit(0);
    }
    printf("ShmAddr:%d\n",ShmAddr);
}
void Semaphore_P(int num){
    SopsP.sem_num = num;
    if(semop(SemID,&SopsP,1) == -1){
        printf("P操作失败.\n");
        semctl(SemID,0,IPC_RMID,sem_union);
        shmdt(ShmAddr);
        shmctl(ShmID,IPC_RMID,0);
        exit(0);
    }

}
void Semaphore_V(int num){
    SopsV.sem_num = num;
    if(semop(SemID,&SopsV,1) == -1){
        printf("V操作失败.\n");
        semctl(SemID,0,IPC_RMID,sem_union);
        shmdt(ShmAddr);
        shmctl(ShmID,IPC_RMID,0);
        exit(0);
    }
}

//使用CSV文件去读图像和标签，主要使用stringstream和getline方法
static void read_csv(const string& filename, vector<Mat>& images, vector<int>& labels, char separator =';') {
    std::ifstream file(filename.c_str(), ifstream::in);
    if (!file) {
        string error_message ="No valid input file was given, please check the given filename.";
        CV_Error(CV_StsBadArg, error_message);
    }
    string line, path, classlabel;
    while (getline(file, line)) {
        stringstream liness(line);
        getline(liness, path, separator);
        getline(liness, classlabel);
        if(!path.empty()&&!classlabel.empty()) {
            images.push_back(imread(path, 0));
            labels.push_back(atoi(classlabel.c_str()));
        }
    }
}

static void help()
{
    cout << "\nThis program demonstrates the cascade recognizer. Now you can use Haar or LBP features.\n"
            "This classifier can recognize many kinds of rigid objects, once the appropriate classifier is trained.\n"
            "It's most known use is for faces.\n"
            "Usage:\n"
            "./facedetect [--cascade=<cascade_path> this is the primary trained classifier such as frontal face]\n"
               "   [--nested-cascade[=nested_cascade_path this an optional secondary classifier such as eyes]]\n"
               "   [--scale=<image scale greater or equal to 1, try 1.3 for example>]\n"
               "   [--try-flip]\n"
               "   [filename|camera_index]\n\n"
            "see facedetect.cmd for one call:\n"
            "./facedetect --cascade=\"../../data/haarcascades/haarcascade_frontalface_alt.xml\" --nested-cascade=\"../../data/haarcascades/haarcascade_eye_tree_eyeglasses.xml\" --scale=1.3\n\n"
            "During execution:\n\tHit any key to quit.\n"
            "\tUsing OpenCV version " << CV_VERSION << "\n" << endl;
}

void detectAndDraw( Mat& img, CascadeClassifier& cascade,
                    CascadeClassifier& nestedCascade,
                    double scale, bool tryflip )
{
    double t = 0;
    vector<Rect> faces, faces2;
    const static Scalar colors[] =
    {
        Scalar(255,0,0),
        Scalar(255,128,0),
        Scalar(255,255,0),
        Scalar(0,255,0),
        Scalar(0,128,255),
        Scalar(0,255,255),
        Scalar(0,0,255),
        Scalar(255,0,255)
    };
    Mat gray, smallImg;
    IsFace = 0;
    FaceSentence = 0;

    cvtColor( img, gray, COLOR_BGR2GRAY );
    double fx = 1 / scale;
    resize( gray, smallImg, Size(), fx, fx, INTER_LINEAR );
    equalizeHist( smallImg, smallImg );

    t = (double)getTickCount();
    cascade.detectMultiScale( smallImg, faces,
        1.1, 2, 0
        //|CASCADE_FIND_BIGGEST_OBJECT
        //|CASCADE_DO_ROUGH_SEARCH
        |CASCADE_SCALE_IMAGE,
        Size(30, 30) );
    if( tryflip )
    {
        flip(smallImg, smallImg, 1);
        cascade.detectMultiScale( smallImg, faces2,
                                 1.1, 2, 0
                                 //|CASCADE_FIND_BIGGEST_OBJECT
                                 //|CASCADE_DO_ROUGH_SEARCH
                                 |CASCADE_SCALE_IMAGE,
                                 Size(30, 30) );
        for( vector<Rect>::const_iterator r = faces2.begin(); r != faces2.end(); ++r )
        {
            faces.push_back(Rect(smallImg.cols - r->x - r->width, r->y, r->width, r->height));
        }
    }
    t = (double)getTickCount() - t;
    //printf( "detection time = %g ms\n", t*1000/getTickFrequency());
    for ( size_t i = 0; i < faces.size(); i++ )
    {
        Rect r = faces[i];
        Mat smallImgROI;
        vector<Rect> nestedObjects;
        Point center;
        Scalar color = colors[i%8];
        int radius;

        double aspect_ratio = (double)r.width/r.height;
        if( 0.75 < aspect_ratio && aspect_ratio < 1.3 )
        {
            center.x = cvRound((r.x + r.width*0.5)*scale);
            center.y = cvRound((r.y + r.height*0.5)*scale);
            radius = cvRound((r.width + r.height)*0.25*scale);
            circle( img, center, radius, color, 3, 8, 0 );
            FaceSentence = 1;
            IsFace = 1;
        }
        else{
            rectangle( img, cvPoint(cvRound(r.x*scale), cvRound(r.y*scale)),
                       cvPoint(cvRound((r.x + r.width-1)*scale), cvRound((r.y + r.height-1)*scale)),
                       color, 3, 8, 0);
        }
        if( nestedCascade.empty() )
            continue;
        smallImgROI = smallImg( r );
        nestedCascade.detectMultiScale( smallImgROI, nestedObjects,
            1.1, 2, 0
            //|CASCADE_FIND_BIGGEST_OBJECT
            //|CASCADE_DO_ROUGH_SEARCH
            //|CASCADE_DO_CANNY_PRUNING
            |CASCADE_SCALE_IMAGE,
            Size(30, 30) );
        for ( size_t j = 0; j < nestedObjects.size(); j++ )
        {
            Rect nr = nestedObjects[j];
            center.x = cvRound((r.x + nr.x + nr.width*0.5)*scale);
            center.y = cvRound((r.y + nr.y + nr.height*0.5)*scale);
            radius = cvRound((nr.width + nr.height)*0.25*scale);
            circle( img, center, radius, color, 3, 8, 0 );
            if(FaceSentence) {
                IsFace = 1;
            }
        }
    }
    //imshow( "result", img );
}

void SendMessage(char *msg){

}

int main(int argc, char *argv[])
{
    SemInit();
    ShmInit();
    //变量声明
      int i = 10;
      //CvCapture* pCapture = cvCreateCameraCapture(0);//摄像头用
      cv::CommandLineParser parser(argc, argv,
          "{help h||}"
          "{cascade|/home/jellal/OPENCV_test/data/haarcascades/haarcascade_frontalface_alt.xml|}"
          "{nested-cascade|/home/jellal/OPENCV_test/data/haarcascades/haarcascade_eye_tree_eyeglasses.xml|}"
          "{scale|1|}{try-flip||}{@filename||}"
      );
      CascadeClassifier cascade, nestedCascade;
      double scale = parser.get<double>("scale");
      if (scale < 1) scale = 1;
      bool tryflip = parser.has("try-flip");
      vector<Mat> images;
      vector<int> labels;
      string fn_csv = string("/home/jellal/at.txt");
      VideoCapture cap;
      Mat Camera_CImg;
      Mat Camera_GImg;
      unsigned char ComInfo;
      int Predicte = -1;
      int NoFaceFlag = 0;
      char c;
      int cnt = 100;
    //处理
      cap.open("rtmp://101.132.135.14/myapp/test1");
      cap.set(CV_CAP_PROP_FRAME_HEIGHT,768);
      cap.set(CV_CAP_PROP_FRAME_WIDTH,1024);

      try {
        read_csv(fn_csv, images, labels);
      }
      catch (cv::Exception& e) {
        cerr <<"Error opening file \""<< fn_csv <<"\". Reason: "<< e.msg << endl;
        // 文件有问题，我们啥也做不了了，退出了
        exit(1);
      }
      if(images.size()<=1) {
              string error_message ="This demo needs at least 2 images to work. Please add more images to your data set!";
              CV_Error(CV_StsError, error_message);
      }
      int height = images[0].rows;
      int width = images[0].cols;
      cout << height <<" "<< width << endl;
      Mat testSample = images[images.size() -1];
      int testLabel = labels[labels.size() -1];
      images.pop_back();
      labels.pop_back();
      Ptr< EigenFaceRecognizer > model = EigenFaceRecognizer::create();
      //训练模型
      //model->train(images, labels);
      //model->write("Model0.yml");
      //加载模型
      model->read("Model0.yml");
      int predictedLabel = model->predict(testSample);
      string result_message = format("Predicted class = %d / Actual class = %d.", predictedLabel, testLabel);
      cout << result_message << endl;
//      //图像展示
//      Mat eigenvalues = model->getEigenVectors();
//      Mat W = model->getEigenVectors();
//      Mat mean = model->getMean();
//      imshow("mean", norm_0_255(mean.reshape(1, images[0].rows)));
//      for (int i =0; i < min(16, W.cols); i++) {
//          string msg = format("Eigenvalue #%d = %.5f", i, eigenvalues.at<double>(i));
//          cout << msg << endl;
//          // 得到第 #i个特征
//          Mat ev = W.col(i).clone();
//          //把它变成原始大小，为了把数据显示归一化到0~255.
//          Mat grayscale = norm_0_255(ev.reshape(1, height));
//          // 使用伪彩色来显示结果，为了更好的感受.
//          Mat cgrayscale;
//          applyColorMap(grayscale, cgrayscale, COLORMAP_JET);

//          imshow(format("eigenface_%d", i), cgrayscale);
//      }
//      for(int num_components = 0; num_components < min(16, W.cols); num_components++) {
//          // 从模型中的特征向量截取一部分
//          Mat evs = W.col(num_components);
//          Mat projection = cv::LDA::subspaceProject(evs, mean, images[0].reshape(1,1));
//          Mat reconstruction = cv::LDA::subspaceReconstruct(evs, mean, projection);
//          // 归一化结果，为了显示:
//          reconstruction = norm_0_255(reconstruction.reshape(1, images[0].rows));

//          imshow(format("fisherface_reconstruction_%d", num_components), reconstruction);
//      }
//      waitKey(0);

      cascadeName = parser.get<string>("cascade");
      nestedCascadeName = parser.get<string>("nested-cascade");
      if( !cascade.load( cascadeName ) )
      {
          cerr << "ERROR: Could not load classifier cascade" << endl;
          help();
          return -1;
      }
      if ( !nestedCascade.load( nestedCascadeName ) )
      {
          cerr << "WARNING: Could not load classifier cascade for nested objects" << endl;
      }
      ComInfo = 120;
      //窗口
      //namedWindow("src");
      //namedWindow("result");
      //namedWindow("gray");
      //主循环
      cout<< "Start detect\n" << endl;
      while(1)
      {
          if(cnt>0) cnt--;
          Semaphore_P(0);
          ComInfo = ShmAddr->num[0];
          Semaphore_V(0);
          if(ComInfo == 1 && cnt == 0){
              printf("有阻挡物 开始识别\n");
              cnt = 100;
              cap>> Camera_CImg;
              if(Camera_CImg.empty()) {
            	  printf("Empty picture\n");
            	  cap.open("rtmp://101.132.135.14/myapp/test1");
            	  cap.set(CV_CAP_PROP_FRAME_HEIGHT,768);
            	  cap.set(CV_CAP_PROP_FRAME_WIDTH,1024);
            	  continue;
              }
              //IplImage* pFrame=cvQueryFrame( pCapture );//摄像头用
              //if(!pFrame)break;//摄像头用
              //Mat frame = cvarrToMat(pFrame,true); //摄像头RGB图像 //摄像头用
              //Mat capframe = cvarrToMat(capFrame,true);
              //lMlat frame1=images[0].clone();
              //cv::resize(frame,frame1,images[0].size(),0,0,CV_INTER_LINEAR); //改变摄像头图像尺寸 //摄像头用
              //cvtColor(frame1,frame1,CV_BGR2GRAY); //变RGB为灰度图像
              cv::resize(Camera_CImg,Camera_GImg,images[0].size(),0,0,CV_INTER_LINEAR);
              detectAndDraw( Camera_GImg, cascade, nestedCascade, scale, tryflip );
              Predicte = 1;
              if(IsFace){
                  NoFaceFlag = 0;
                  cvtColor(Camera_GImg,Camera_GImg,CV_BGR2GRAY);
                  Predicte = model->predict(Camera_GImg);
                  imwrite(format("/home/jellal/Downloads/orl_faces/s41/%d.pgm",99),Camera_CImg);
                  cap.open("rtmp://101.132.135.14/myapp/test1");
                  Semaphore_P(0);
                  ShmAddr->num[0] = 0;
                  Semaphore_V(0);
              }
              else{
                  NoFaceFlag++;
                  if(NoFaceFlag == 300){
                      NoFaceFlag = 0;
                      cout<< "Something is in the front of door" << endl;
                      SendMessage("Something is in the front of door");
                  }
              }
              if(Predicte == 41 && IsFace) {
                  cout<< "Accepted" << endl;
                  Semaphore_P(1);
                  printf("Visiontest\n");
                  ShmAddr->num[1] = 1;
                  Semaphore_V(1);
              }
              if(Predicte != 41 && IsFace) {
                  cout<< "Dangerouse" << endl;
                  Semaphore_P(1);
                  printf("Visiontest\n");
                  ShmAddr->num[1] = 0;
                  Semaphore_V(1);
                  SendMessage("Dangerouse");
              }
          }
//          c = cvWaitKey(33);
//          if(c == 27)   break;
//          else if(c == 'p'){
//              i++;
//              printf("%d\n",i);
//              imwrite(format("/home/jellal/Downloads/orl_faces/s41/%d.pgm",i),Camera_GImg);
//          }
//          else if(c == 'q'){
//              break;
//          }
//          else if(c == 'e'){
//              ComInfo = 120;
//          }
      }

      //cvReleaseCapture(&pCapture);
      //cvDestroyWindow("Video");
      return 0;
}

