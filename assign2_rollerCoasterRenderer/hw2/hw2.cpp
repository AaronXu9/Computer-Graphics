/*
  CSCI 420 Computer Graphics, USC
  Assignment 2: Roller Coaster
  C++ starter code
  Student username: Ao Xu
*/

#include <iostream>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "openGLHeader.h"
#include "glutHeader.h"
#include "imageIO.h"
#include "openGLMatrix.h"
#include "texPipelineProgram.h"
#include "basicPipelineProgram.h"

#ifdef WIN32
  #ifdef _DEBUG
    #pragma comment(lib, "glew32d.lib")
  #else
    #pragma comment(lib, "glew32.lib")
  #endif
#endif

#ifdef WIN32
  char shaderBasePath[1024] = SHADER_BASE_PATH;
#else
  char shaderBasePath[1024] = "../openGLHelper-starterCode";
#endif

using namespace std;

// represents one control point along the spline
struct Point
{
  double x;
  double y;
  double z;
};
// spline struct
// contains how many control points the spline has, and an array of control points
struct Spline
{
  int numControlPoints;
  Point * points;
};

int mousePos[2]; // x,y coordinate of the mouse position

int leftMouseButton = 0; // 1 if pressed, 0 if not
int middleMouseButton = 0; // 1 if pressed, 0 if not
int rightMouseButton = 0; // 1 if pressed, 0 if not

typedef enum { ROTATE, TRANSLATE, SCALE } CONTROL_STATE;
CONTROL_STATE controlState = ROTATE;

// state of the world
float landRotate[3] = { 0.0f, 0.0f, 0.0f };
float landTranslate[3] = { 0.0f, 0.0f, 0.0f };
float landScale[3] = { 1.0f, 1.0f, 1.0f };

int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 homework II";

GLuint program;
OpenGLMatrix * matrix;
TexPipelineProgram * pipelineProgram;
BasicPipelineProgram * basicPipelineProgram;
GLuint trackbuffer, skyBuffer, groundBuffer, crossBarBuffer;
GLuint trackVAO, skyVAO, groundVAO, crossBarVAO;

GLuint skyTexHandle;
vector<float> skyPosition;
vector<float> skyUVs;

GLuint groundTexHandle;
vector<float> groundPosition;
vector<float> groundUVs;

GLuint trackTexHandle;
vector<float> trackPosition;
vector<float> trackUVs;

GLuint crossBarTexHandle;
vector<float> crossBarPosition;
vector<float> crossBarUVs;

float FOV = 90.0;
float eye[3] = {0, 0, 0};
float focus[3] = {0, 0, -1};
float up[3] = {0, 1, 0};
//increase u by step
float u = 0;
float step = 0.001;
//moveindex to control step, float to control speed
int cameraMoveIndex = 0;
int controlPoint = 0;
int splineNum = 0;
Spline * splines;
int numSplines;
//the  Catmull-Rom matrix
static const float s = 0.5;
float M [4][4] = {{(-s),  (2-s),  (s-2),   (s)},
                  {(2*s), (s-3),  (3-2*s),  (-s)},
                  {(-s),  (0),    (s),    (0)},
                  {(0),   (1),    (0),   (0)}};

bool screenShotFlag=true; //false to stop screenshotting
int screenshotCounter=0;
//time interval for taking screenshots
int timer = 1000;

vector<Point> splineCoord;
vector<Point> tangentCoord;

Point tangent;
Point normal;
Point binormal;
// write a screenshot to the specified filename
void saveScreenshot(const char * filename)
{
  unsigned char * screenshotData = new unsigned char[windowWidth * windowHeight * 3];
  glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, screenshotData);

  ImageIO screenshotImg(windowWidth, windowHeight, 3, screenshotData);

  if (screenshotImg.save(filename, ImageIO::FORMAT_JPEG) == ImageIO::OK)
    cout << "File " << filename << " saved successfully." << endl;
  else cout << "Failed to save file " << filename << '.' << endl;

  delete [] screenshotData;
}

// nomalize v to a unit vector in the same direction 
void normalize(Point &v)
{
    double mag = sqrt(pow(v.x,2) + pow(v.y,2) + pow(v.z,2));
    if (mag>=0)
    {
      v.x = v.x / mag;
      v.y = v.y / mag;
      v.z = v.z / mag;
    }
    else
    {
      v.x = 0;
      v.y = 0;
      v.z = 0;
    }
}

// compute the corss product of the vectores a and b and stores in c
void computeCrossProduct(Point a, Point b, Point &c)
{
  c.x = a.y*b.z - b.y*a.z;
  c.y = a.z*b.x - b.z*a.x;
  c.z = a.x*b.y - b.x*a.y;
}

// computes the normal and binormal given the tangent at a point
void computeNormal(Point tangent, Point &normal, Point &binormal)
{
  computeCrossProduct(binormal, tangent, normal);
  normalize(normal);

  computeCrossProduct(tangent, normal, binormal);
  normalize(binormal);
}

// adds a triangle and its corresponding UV values to the vector variables position and uvs - used to add faces of a track
void addTriangle(Point a, float au, float av, Point b, float bu, float bv, Point c, float cu, float cv)
{
  trackPosition.push_back(a.x);
  trackPosition.push_back(a.y);
  trackPosition.push_back(a.z);

  trackUVs.push_back(au);
  trackUVs.push_back(av);

  trackPosition.push_back(b.x);
  trackPosition.push_back(b.y);
  trackPosition.push_back(b.z);

  trackUVs.push_back(bu);
  trackUVs.push_back(bv);

  trackPosition.push_back(c.x);
  trackPosition.push_back(c.y);
  trackPosition.push_back(c.z);

  trackUVs.push_back(cu);
  trackUVs.push_back(cv);
}

//texture shader
void setTextureUnit(GLint unit)
{
  GLuint program = pipelineProgram->GetProgramHandle();
  // select the active texture unit
  glActiveTexture(unit); 
  // get a handle to the “textureImage” shader variable
  GLint h_textureImage = glGetUniformLocation(program, "textureImage");
  // use shader variable “textureImage” to read from texture unit “unit”
  glUniform1i(h_textureImage, unit - GL_TEXTURE0);
}

// initializes the coodinates of the sky box and uv values
void createSky()
{
  //back face
  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyUVs.push_back(0);
  skyUVs.push_back(1);

  skyPosition.push_back(-100);
  skyPosition.push_back(-100);
  skyPosition.push_back(-100);
  skyUVs.push_back(0);
  skyUVs.push_back(0);

  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyPosition.push_back(-100);
  skyUVs.push_back(1);
  skyUVs.push_back(0);

  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyUVs.push_back(0);
  skyUVs.push_back(1);

  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyUVs.push_back(1);
  skyUVs.push_back(1);

  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyPosition.push_back(-100);
  skyUVs.push_back(1);
  skyUVs.push_back(0);

  //front face
  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyUVs.push_back(0);
  skyUVs.push_back(1);

  skyPosition.push_back(-100);
  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyUVs.push_back(0);
  skyUVs.push_back(0);

  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyUVs.push_back(1);
  skyUVs.push_back(0);

  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyUVs.push_back(0);
  skyUVs.push_back(1);

  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyUVs.push_back(1);
  skyUVs.push_back(1);

  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyUVs.push_back(1);
  skyUVs.push_back(0);

  //right face
  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyUVs.push_back(0);
  skyUVs.push_back(1);

  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyUVs.push_back(0);
  skyUVs.push_back(0);

  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyPosition.push_back(-100);
  skyUVs.push_back(1);
  skyUVs.push_back(0);

  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyUVs.push_back(0);
  skyUVs.push_back(1);

  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyUVs.push_back(1);
  skyUVs.push_back(1);

  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyPosition.push_back(-100);
  skyUVs.push_back(1);
  skyUVs.push_back(0);

  //left face
  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyUVs.push_back(0);
  skyUVs.push_back(1);

  skyPosition.push_back(-100);
  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyUVs.push_back(0);
  skyUVs.push_back(0);

  skyPosition.push_back(-100);
  skyPosition.push_back(-100);
  skyPosition.push_back(-100);
  skyUVs.push_back(1);
  skyUVs.push_back(0);

  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyUVs.push_back(0);
  skyUVs.push_back(1);

  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyUVs.push_back(1);
  skyUVs.push_back(1);

  skyPosition.push_back(-100);
  skyPosition.push_back(-100);
  skyPosition.push_back(-100);
  skyUVs.push_back(1);
  skyUVs.push_back(0);

  //top face
  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyUVs.push_back(0);
  skyUVs.push_back(0);

  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyUVs.push_back(0);
  skyUVs.push_back(1);

  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyUVs.push_back(1);
  skyUVs.push_back(0);

  skyPosition.push_back(-100);
  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyUVs.push_back(0);
  skyUVs.push_back(1);

  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyPosition.push_back(-100);
  skyUVs.push_back(1);
  skyUVs.push_back(1);

  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyPosition.push_back(100);
  skyUVs.push_back(1);
  skyUVs.push_back(0);

}

// initializes the ground coodinates and uv values
void createGround()
{


  groundPosition.push_back(-100);
  groundPosition.push_back(-100);
  groundPosition.push_back(-100);
  groundUVs.push_back(0);
  groundUVs.push_back(1);

  groundPosition.push_back(100);
  groundPosition.push_back(-100);
  groundPosition.push_back(-100);
  groundUVs.push_back(1);
  groundUVs.push_back(1);

  groundPosition.push_back(-100);
  groundPosition.push_back(-100);
  groundPosition.push_back(100);
  groundUVs.push_back(0);
  groundUVs.push_back(0);

  groundPosition.push_back(100);
  groundPosition.push_back(-100);
  groundPosition.push_back(-100);
  groundUVs.push_back(1);
  groundUVs.push_back(1);

  groundPosition.push_back(-100);
  groundPosition.push_back(-100);
  groundPosition.push_back(100);
  groundUVs.push_back(0);
  groundUVs.push_back(0);

  groundPosition.push_back(100);
  groundPosition.push_back(-100);
  groundPosition.push_back(100);
  groundUVs.push_back(1);
  groundUVs.push_back(0);

}
// initializes spline coodinates using, creating tracks and crossbars
void createTrack()
{
  float u1 = 0;
  double temp_spline[4];
  double temp_tangent[4];
  Point position, tangent;
  Point prevCoord, prevTangent;
  float C[4][3];

  //iterate through every spline
  for (int j = 0; j < numSplines; j++)
  { 
    for (int i=0;i<splines[j].numControlPoints-3;i++)
    {
      for (int k=0;k<4;k++)
      {
        C[k][0] = splines[j].points[i+k].x;
        C[k][1] = splines[j].points[i+k].y;
        C[k][2] = splines[j].points[i+k].z;
      }
      //compute the position and tangent of a point on the spline
      for(u1=0;u1<=1;u1+=step)
      {
        for (int l=0; l<4;l++)
        {
          temp_spline[l] = (pow(u1,3) * M[0][l]) + (pow(u1,2) * M[1][l]) + (pow(u1,1) * M[2][l]) + (M[3][l]); //p(u)
          temp_tangent[l] = (3 * pow(u1,2) * M[0][l]) + (2 * u1 * M[1][l]) + M[2][l]; //derivative of p(u)
        }

        position.x = 0.0;
        position.y = 0.0;
        position.z = 0.0;

        tangent.x = 0.0;
        tangent.y = 0.0;
        tangent.z = 0.0;

        for (int m=0;m<4;m++)
        {
          position.x += temp_spline[m]*C[m][0];
          position.y += temp_spline[m]*C[m][1];
          position.z += temp_spline[m]*C[m][2];

          tangent.x += temp_tangent[m]*C[m][0];
          tangent.y += temp_tangent[m]*C[m][1];
          tangent.z += temp_tangent[m]*C[m][2];
        }

        splineCoord.push_back(position);
        normalize(tangent);
        tangentCoord.push_back(tangent);
      }
    }
  }

  //the positions for the rails
  Point v0,v1,v2,v3,v4,v5,v6,v7,v;
  //normals and binormals 
  Point n0,n1,b0,b1;
  float alpha = 0.002; //ratio of the normal
  float alpha2 = 0.02; //ratio of the binormal
  float shift = 0.03; //shift to get the other track
  int crossBar=0;

  v.x = 0.00000000000001;
  v.y = 1;
  v.z = 0.00000000000001;

  computeCrossProduct(tangentCoord[0], v, n0);
  normalize(n0);

  computeCrossProduct(tangentCoord[0], n0, b0);
  normalize(b0);

  Point V0,V1,V2,V3,V4,V5,V6,V7;
  Point crossBar0,crossBar1,crossBar2,crossBar3;

  for (int i=1; i<splineCoord.size(); i+=10)
  {
    if (i!=0)
    {
      computeNormal(tangentCoord[i], n0, b0);
    }

    v0.x = splineCoord[i].x + alpha * (-n0.x) + alpha2* (b0.x);
    v0.y = splineCoord[i].y + alpha * (-n0.y) + alpha2* (b0.y);
    v0.z = splineCoord[i].z + alpha * (-n0.z) + alpha2* (b0.z);

    v1.x = splineCoord[i].x + alpha * (n0.x) + alpha2* (b0.x);
    v1.y = splineCoord[i].y + alpha * (n0.y) + alpha2* (b0.y);
    v1.z = splineCoord[i].z + alpha * (n0.z) + alpha2* (b0.z);

    v2.x = splineCoord[i].x + alpha * (n0.x) - alpha2* (b0.x);
    v2.y = splineCoord[i].y + alpha * (n0.y) - alpha2* (b0.y);
    v2.z = splineCoord[i].z + alpha * (n0.z) - alpha2* (b0.z);

    v3.x = splineCoord[i].x + alpha * (-n0.x) - alpha2* (b0.x);
    v3.y = splineCoord[i].y + alpha * (-n0.y) - alpha2* (b0.y);
    v3.z = splineCoord[i].z + alpha * (-n0.z) - alpha2* (b0.z);

    b1 = b0;
    computeNormal(tangentCoord[i+10], n1, b1);

    v4.x = splineCoord[i+10].x + alpha * (-n1.x) + alpha2* (b1.x);
    v4.y = splineCoord[i+10].y + alpha * (-n1.y) + alpha2* (b1.y);
    v4.z = splineCoord[i+10].z + alpha * (-n1.z) + alpha2* (b1.z);

    v5.x = splineCoord[i+10].x + alpha * (n1.x) + alpha2* (b1.x);
    v5.y = splineCoord[i+10].y + alpha * (n1.y) + alpha2* (b1.y);
    v5.z = splineCoord[i+10].z + alpha * (n1.z) + alpha2* (b1.z);

    v6.x = splineCoord[i+10].x + alpha * (n1.x) - alpha2* (b1.x);
    v6.y = splineCoord[i+10].y + alpha * (n1.y) - alpha2* (b1.y);
    v6.z = splineCoord[i+10].z + alpha * (n1.z) - alpha2* (b1.z);

    v7.x = splineCoord[i+10].x + alpha * (-n1.x) - alpha2* (b1.x);
    v7.y = splineCoord[i+10].y + alpha * (-n1.y) - alpha2* (b1.y);
    v7.z = splineCoord[i+10].z + alpha * (-n1.z) - alpha2* (b1.z);

    //positions for the left rail
    V0.x = v3.x;
    V0.y = v3.y;
    V0.z = v3.z;

    V1.x = v2.x;
    V1.y = v2.y;
    V1.z = v2.z;

    V2.x = v2.x - shift*b0.x;
    V2.y = v2.y - shift*b0.y;
    V2.z = v2.z - shift*b0.z;

    V3.x = v3.x - shift*b0.x;
    V3.y = v3.y - shift*b0.y;
    V3.z = v3.z - shift*b0.z;

    V4.x = v7.x;
    V4.y = v7.y;
    V4.z = v7.z;

    V5.x = v6.x;
    V5.y = v6.y;
    V5.z = v6.z;

    V6.x = v6.x - shift*b1.x;
    V6.y = v6.y - shift*b1.y;
    V6.z = v6.z - shift*b1.z;

    V7.x = v7.x - shift*b1.x;
    V7.y = v7.y - shift*b1.y;
    V7.z = v7.z - shift*b1.z;
    //create the cube by adding two triangles
    //right face  
    addTriangle(V6, 0, 1, V2, 0, 0, V1, 1, 0);
    addTriangle(V6, 0, 1, V5, 1, 1, V1, 1, 0);
    //top face
    addTriangle(V5, 0, 1, V1, 0, 0, V0, 1, 0);
    addTriangle(V5, 0, 1, V4, 1, 1, V0, 1, 0);
    //left face
    addTriangle(V7, 0, 1, V3, 0, 0, V0, 1, 0);
    addTriangle(V7, 0, 1, V4, 1, 1, V0, 1, 0);
    //bottom face
    addTriangle(V6, 0, 1, V2, 0, 0, V3, 1, 0);
    addTriangle(V6, 0, 1, V7, 1, 1, V3, 1, 0);

    //positions for the right rail
    V0.x = v0.x + shift*b0.x;
    V0.y = v0.y + shift*b0.y;
    V0.z = v0.z + shift*b0.z;

    V1.x = v1.x + shift*b0.x;
    V1.y = v1.y + shift*b0.y;
    V1.z = v1.z + shift*b0.z;

    V2.x = v1.x;
    V2.y = v1.y;
    V2.z = v1.z;

    V3.x = v0.x;
    V3.y = v0.y;
    V3.z = v0.z;

    V4.x = v4.x + shift*b1.x;
    V4.y = v4.y + shift*b1.y;
    V4.z = v4.z + shift*b1.z;

    V5.x = v5.x + shift*b1.x;
    V5.y = v5.y + shift*b1.y;
    V5.z = v5.z + shift*b1.z;

    V6.x = v5.x;
    V6.y = v5.y;
    V6.z = v5.z;

    V7.x = v4.x;
    V7.y = v4.y;
    V7.z = v4.z;

    //right face
    addTriangle(V6, 0, 1, V2, 0, 0, V1, 1, 0);
    addTriangle(V6, 0, 1, V5, 1, 1, V1, 1, 0);
    //top face
    addTriangle(V5, 0, 1, V1, 0, 0, V0, 1, 0);
    addTriangle(V5, 0, 1, V4, 1, 1, V0, 1, 0);
    //left face
    addTriangle(V7, 0, 1, V3, 0, 0, V0, 1, 0);
    addTriangle(V7, 0, 1, V4, 1, 1, V0, 1, 0);
    //bottom face
    addTriangle(V6, 0, 1, V2, 0, 0, V3, 1, 0);
    addTriangle(V6, 0, 1, V7, 1, 1, V3, 1, 0);

    //positions of the cross bars
    crossBar0 = v0;
    crossBar3 = v3;

    crossBar1.x = v0.x + 0.01*tangentCoord[i].x;
    crossBar1.y = v0.y + 0.01*tangentCoord[i].y;
    crossBar1.z = v0.z + 0.01*tangentCoord[i].z;

    crossBar2.x = v3.x + 0.01*tangentCoord[i].x;
    crossBar2.y = v3.y + 0.01*tangentCoord[i].y;
    crossBar2.z = v3.z + 0.01*tangentCoord[i].z;

    crossBarPosition.push_back(crossBar2.x);
    crossBarPosition.push_back(crossBar2.y);
    crossBarPosition.push_back(crossBar2.z);

    crossBarUVs.push_back(0);
    crossBarUVs.push_back(1);

    crossBarPosition.push_back(crossBar1.x);
    crossBarPosition.push_back(crossBar1.y);
    crossBarPosition.push_back(crossBar1.z);

    crossBarUVs.push_back(1);
    crossBarUVs.push_back(1);

    crossBarPosition.push_back(crossBar0.x);
    crossBarPosition.push_back(crossBar0.y);
    crossBarPosition.push_back(crossBar0.z);

    crossBarUVs.push_back(1);
    crossBarUVs.push_back(0);

    crossBarPosition.push_back(crossBar2.x);
    crossBarPosition.push_back(crossBar2.y);
    crossBarPosition.push_back(crossBar2.z);

    crossBarUVs.push_back(0);
    crossBarUVs.push_back(1);

    crossBarPosition.push_back(crossBar3.x);
    crossBarPosition.push_back(crossBar3.y);
    crossBarPosition.push_back(crossBar3.z);

    crossBarUVs.push_back(0);
    crossBarUVs.push_back(0);

    crossBarPosition.push_back(crossBar0.x);
    crossBarPosition.push_back(crossBar0.y);
    crossBarPosition.push_back(crossBar0.z);

    crossBarUVs.push_back(1);
    crossBarUVs.push_back(0);

    b0 = b1;

  }
}


void initVBOs()
{
  //vbo for the rails
  glGenBuffers(1, &trackbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, trackbuffer);
  glBufferData(GL_ARRAY_BUFFER, (trackPosition.size() + trackUVs.size()) * sizeof(float), NULL, GL_STATIC_DRAW);
  // upload position data
  glBufferSubData(GL_ARRAY_BUFFER, 0, trackPosition.size() * sizeof(float), trackPosition.data());
  // upload uv data
  glBufferSubData(GL_ARRAY_BUFFER, trackPosition.size() * sizeof(float), trackUVs.size() * sizeof(float), trackUVs.data());

  //vbo for the crossbar
  glGenBuffers(1, &crossBarBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, crossBarBuffer);
  glBufferData(GL_ARRAY_BUFFER, (crossBarPosition.size() + crossBarUVs.size()) * sizeof(float), NULL, GL_STATIC_DRAW);
  // upload position data
  glBufferSubData(GL_ARRAY_BUFFER, 0, crossBarPosition.size() * sizeof(float), crossBarPosition.data());
  // upload uv data
  glBufferSubData(GL_ARRAY_BUFFER, crossBarPosition.size() * sizeof(float), crossBarUVs.size() * sizeof(float), crossBarUVs.data());

  //vbo for the ground
  glGenBuffers(1, &groundBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, groundBuffer);
  glBufferData(GL_ARRAY_BUFFER, (groundPosition.size() + groundUVs.size()) * sizeof(float), NULL, GL_STATIC_DRAW);
  // upload position data
  glBufferSubData(GL_ARRAY_BUFFER, 0, groundPosition.size() * sizeof(float), groundPosition.data());
  // upload uv data
  glBufferSubData(GL_ARRAY_BUFFER, groundPosition.size() * sizeof(float), groundUVs.size() * sizeof(float), groundUVs.data());

  //vbo for the sky box
  glGenBuffers(1, &skyBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, skyBuffer);
  glBufferData(GL_ARRAY_BUFFER, (skyPosition.size() + skyUVs.size()) * sizeof(float), NULL, GL_STATIC_DRAW);
  // upload position data
  glBufferSubData(GL_ARRAY_BUFFER, 0, skyPosition.size() * sizeof(float), skyPosition.data());
  // upload uv data
  glBufferSubData(GL_ARRAY_BUFFER, skyPosition.size() * sizeof(float), skyUVs.size() * sizeof(float), skyUVs.data());

}

// sets the camera attributes eye, focus and up
void setCamera(int i)
{
  computeNormal(tangentCoord[i], normal, binormal);

  eye[0] = splineCoord[i].x + 0.03 * normal.x;
  eye[1] = splineCoord[i].y + 0.03 * normal.y;
  eye[2] = splineCoord[i].z + 0.03 * normal.z;

  focus[0] = splineCoord[i+1].x + 0.03 * normal.x;
  focus[1] = splineCoord[i+1].y + 0.03 * normal.y;
  focus[2] = splineCoord[i+1].z + 0.03 * normal.z;

  up[0] = normal.x;
  up[1] = normal.y;
  up[2] = normal.z;
}

// displays the ground
void displayGround()
{
  glGenVertexArrays(1, &groundVAO);

  setTextureUnit(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, groundTexHandle);

  GLuint program = pipelineProgram->GetProgramHandle();
  glBindVertexArray(groundVAO);
  glBindBuffer(GL_ARRAY_BUFFER, groundBuffer);
  GLuint loc = glGetAttribLocation(program, "position");
  glEnableVertexAttribArray(loc);
  const void * offset = (const void*) 0;
  glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, offset);

  GLuint loc2 = glGetAttribLocation(program, "texCoord");
  glEnableVertexAttribArray(loc2);
  const void * offset2 = (const void*) (size_t)(groundPosition.size()*sizeof(float));
  glVertexAttribPointer(loc2, 2, GL_FLOAT, GL_FALSE, 0, offset2);
  glBindVertexArray(0);

  glBindVertexArray(groundVAO);
  GLint first = 0;
  GLsizei numberOfVertices = (groundPosition.size()/3);
  glDrawArrays(GL_TRIANGLES, first, numberOfVertices);

  glBindVertexArray(0);
}

// displays the sky
void displaySky()
{
  glGenVertexArrays(1, &skyVAO);

  setTextureUnit(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, skyTexHandle);

  GLuint program = pipelineProgram->GetProgramHandle();
  glBindVertexArray(skyVAO);
  glBindBuffer(GL_ARRAY_BUFFER, skyBuffer);
  GLuint loc = glGetAttribLocation(program, "position");
  glEnableVertexAttribArray(loc);
  const void * offset = (const void*) 0;
  glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, offset);

  GLuint loc2 = glGetAttribLocation(program, "texCoord");
  glEnableVertexAttribArray(loc2);
  const void * offset2 = (const void*) (size_t)(skyPosition.size()*sizeof(float));
  glVertexAttribPointer(loc2, 2, GL_FLOAT, GL_FALSE, 0, offset2);
  glBindVertexArray(0);

  glBindVertexArray(skyVAO);
  GLint first = 0;
  GLsizei numberOfVertices = (skyPosition.size()/3);
  glDrawArrays(GL_TRIANGLES, first, numberOfVertices);

  glBindVertexArray(0);
}

// displays the track and crossbars
void displayTrack()
{
  //bind vao for track
  glGenVertexArrays(1, &trackVAO);

  setTextureUnit(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, trackTexHandle);

  GLuint program = pipelineProgram->GetProgramHandle();
  glBindVertexArray(trackVAO);
  glBindBuffer(GL_ARRAY_BUFFER, trackbuffer);
  GLuint loc = glGetAttribLocation(program, "position");
  glEnableVertexAttribArray(loc);
  const void * offset = (const void*) 0;
  glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, offset);

  GLuint loc2 = glGetAttribLocation(program, "texCoord");
  glEnableVertexAttribArray(loc2);
  const void * offset2 = (const void*) (size_t)(trackPosition.size()*sizeof(float));
  glVertexAttribPointer(loc2, 2, GL_FLOAT, GL_FALSE, 0, offset2);
  glBindVertexArray(0);

  glBindVertexArray(trackVAO);
  GLint first = 0;
  GLsizei numberOfVertices = (trackPosition.size()/3);
  glDrawArrays(GL_TRIANGLES, first, numberOfVertices);
  glBindVertexArray(0);


  //bind vao for cross bars
  glGenVertexArrays(1, &crossBarVAO);
  setTextureUnit(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, crossBarTexHandle);

  program = pipelineProgram->GetProgramHandle();
  glBindVertexArray(crossBarVAO);
  glBindBuffer(GL_ARRAY_BUFFER, crossBarBuffer);
  GLuint loc3 = glGetAttribLocation(program, "position");
  glEnableVertexAttribArray(loc3);
  const void * offset3 = (const void*) 0;
  glVertexAttribPointer(loc3, 3, GL_FLOAT, GL_FALSE, 0, offset3);

  GLuint loc4 = glGetAttribLocation(program, "texCoord");
  glEnableVertexAttribArray(loc4);
  const void * offset4 = (const void*) (size_t)(crossBarPosition.size()*sizeof(float));
  glVertexAttribPointer(loc4, 2, GL_FLOAT, GL_FALSE, 0, offset4);
  glBindVertexArray(0);

  glBindVertexArray(crossBarVAO);
   first = 0;
   numberOfVertices = (crossBarPosition.size()/3);
  glDrawArrays(GL_TRIANGLES, first, numberOfVertices);
  glBindVertexArray(0);

}

void setLight() {
  // pass uniforms to shaders to render light
  program = basicPipelineProgram->GetProgramHandle();
  
  GLint h_normalMatrix = glGetUniformLocation(program, "normalMatrix");
  float n[16];
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);
  matrix->GetNormalMatrix(n); // get normal matrix
  glUniformMatrix4fv(h_normalMatrix, 1, GL_FALSE, n);

  GLint h_lightAmbient = glGetUniformLocation(program, "lightAmbient");
  float lightAmbient[4] = { 1.0f, 1.0f, 1.0f, 1.0f};
  glUniform4fv(h_lightAmbient, 1, lightAmbient);

  GLint h_lightDiffuse = glGetUniformLocation(program, "lightDiffuse");
  float lightDiffuse[4] = { 1.0f, 0.0f, 1.0f, 1.0f };
  glUniform4fv(h_lightDiffuse, 1, lightDiffuse);

  GLint h_lightSpecular = glGetUniformLocation(program, "lightSpecular");
  float lightSpecular[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
  glUniform4fv(h_lightSpecular, 1, lightSpecular);

  GLint h_lightPosition = glGetUniformLocation(program, "lightPosition");
  float lightPosition[3] = { -64.0f, 48.0f, -32.0f};
  glUniform3fv(h_lightPosition, 1, lightPosition);

  GLint h_matKa = glGetUniformLocation(program, "matKa");
  float matKa = 1.0f;
  glUniform1f(h_matKa, matKa);

  GLint h_matKd = glGetUniformLocation(program, "matKd");
  float matKd = 1.2f;
  glUniform1f(h_matKd, matKd);

  GLint h_matKs = glGetUniformLocation(program, "matKs");
  float matKs = 1.6f;
  glUniform1f(h_matKs, matKs);

  GLint h_matKsExp = glGetUniformLocation(program, "matKsExp");
  float matKsExp = 3.0f;
  glUniform1f(h_matKsExp, matKsExp);
}

void display()
{
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  pipelineProgram->Bind();

  setCamera(cameraMoveIndex);
  setLight();
  cameraMoveIndex++;
  u += step;
  //update u_value 
  if (u>1)
  {
    u = 0;
    controlPoint++;
    if (controlPoint>splines[splineNum].numControlPoints-3)
    {
      controlPoint = 0;
      splineNum++;
      if (splineNum>=numSplines)
      {
        exit(0);
      }
    }
  }

  program = pipelineProgram->GetProgramHandle();
  //projection matrix
  GLint h_projectionMatrix = glGetUniformLocation(program, "projectionMatrix");
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->LoadIdentity();
  matrix->Perspective(FOV, (1.0*windowWidth/windowHeight), 0.01, 1000.0);
  float p[16];
  matrix->GetMatrix(p);
  pipelineProgram->Bind();
  glUniformMatrix4fv(h_projectionMatrix, 1, GL_FALSE, p);

  //modelview matrix
  GLint h_modelViewMatrix = glGetUniformLocation(program, "modelViewMatrix");
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);
  matrix->LoadIdentity();
  matrix->LookAt(eye[0], eye[1], eye[2], focus[0], focus[1], focus[2], up[0], up[1], up[2]); // eye, focus, up
  matrix->Translate(landTranslate[0],landTranslate[1],landTranslate[2]);
  matrix->Rotate(landRotate[0],1,0,0);
  matrix->Rotate(landRotate[1],0,1,0);
  matrix->Rotate(landRotate[2],0,0,1);
  matrix->Scale(landScale[0],landScale[1],landScale[2]);

  float m[16];
  matrix->GetMatrix(m);
  pipelineProgram->Bind();
  glUniformMatrix4fv(h_modelViewMatrix, 1, GL_FALSE, m);

  displayGround();
  displaySky();
  displayTrack();
  setLight();
  glutSwapBuffers();
}

int initTexture(const char * imageFilename, GLuint textureHandle)
{
  // read the texture image
  ImageIO img;
  ImageIO::fileFormatType imgFormat;
  ImageIO::errorType err = img.load(imageFilename, &imgFormat);

  if (err != ImageIO::OK)
  {
    printf("Loading texture from %s failed.\n", imageFilename);
    return -1;
  }

  // check that the number of bytes is a multiple of 4
  if (img.getWidth() * img.getBytesPerPixel() % 4)
  {
    printf("Error (%s): The width*numChannels in the loaded image must be a multiple of 4.\n", imageFilename);
    return -1;
  }

  // allocate space for an array of pixels
  int width = img.getWidth();
  int height = img.getHeight();
  unsigned char * pixelsRGBA = new unsigned char[4 * width * height]; // we will use 4 bytes per pixel, i.e., RGBA

  // fill the pixelsRGBA array with the image pixels
  memset(pixelsRGBA, 0, 4 * width * height); // set all bytes to 0
  for (int h = 0; h < height; h++)
    for (int w = 0; w < width; w++)
    {
      // assign some default byte values (for the case where img.getBytesPerPixel() < 4)
      pixelsRGBA[4 * (h * width + w) + 0] = 0; // red
      pixelsRGBA[4 * (h * width + w) + 1] = 0; // green
      pixelsRGBA[4 * (h * width + w) + 2] = 0; // blue
      pixelsRGBA[4 * (h * width + w) + 3] = 255; // alpha channel; fully opaque

      // set the RGBA channels, based on the loaded image
      int numChannels = img.getBytesPerPixel();
      for (int c = 0; c < numChannels; c++) // only set as many channels as are available in the loaded image; the rest get the default value
        pixelsRGBA[4 * (h * width + w) + c] = img.getPixel(w, h, c);
    }

  // bind the texture
  glBindTexture(GL_TEXTURE_2D, textureHandle);

  // initialize the texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsRGBA);

  // generate the mipmaps for this texture
  glGenerateMipmap(GL_TEXTURE_2D);

  // set the texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  // query support for anisotropic texture filtering
  GLfloat fLargest;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
  printf("Max available anisotropic samples: %f\n", fLargest);
  // set anisotropic texture filtering
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0.5f * fLargest);

  // query for any errors
  GLenum errCode = glGetError();
  if (errCode != 0)
  {
    printf("Texture initialization error. Error code: %d.\n", errCode);
    return -1;
  }

  // de-allocate the pixel array -- it is no longer needed
  delete [] pixelsRGBA;

  return 0;
}

void initScene(int argc, char *argv[])
{
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glEnable (GL_DEPTH_TEST);

  glGenTextures(1, &skyTexHandle);
  int code = initTexture("textures/sky.jpg", skyTexHandle);
  if (code != 0)
  {
    printf("Error loading the sky texture image.\n");
    exit(EXIT_FAILURE);
  }

  glGenTextures(1, &groundTexHandle);
  code = initTexture("textures/ground.jpg", groundTexHandle);
  if (code != 0)
  {
    printf("Error loading the ground texture image.\n");
    exit(EXIT_FAILURE);
  }

  glGenTextures(1, &trackTexHandle);
  code = initTexture("textures/track.jpg", trackTexHandle);
  if (code != 0)
  {
    printf("Error loading the ground texture image.\n");
    exit(EXIT_FAILURE);
  }

  glGenTextures(1, &crossBarTexHandle);
  code = initTexture("textures/crossBar.jpg", crossBarTexHandle);
  if (code != 0)
  {
    printf("Error loading the ground texture image.\n");
    exit(EXIT_FAILURE);
  }

  pipelineProgram = new TexPipelineProgram();
  pipelineProgram->Init("../openGLHelper-starterCode");
  basicPipelineProgram = new BasicPipelineProgram();
  basicPipelineProgram->Init("../openGLHelper-starterCode");
  matrix = new OpenGLMatrix();
  createGround();
  createSky();
  createTrack();
  initVBOs();

  //starting point
  Point v;
  v.x = 0.0000000001;
  v.y = -1;
  v.z = 0.0000000001;

  tangent.x = tangentCoord[0].x;
  tangent.y = tangentCoord[0].y;
  tangent.z = tangentCoord[0].z;

  computeCrossProduct(tangent,v,normal);
  normalize(normal);
  computeCrossProduct(tangent,normal,binormal);
  normalize(binormal);

}

void idleFunc()
{
  // for example, here, you can save the screenshots to disk (to make the animation)
  glutPostRedisplay();
}


void reshapeFunc(int w, int h)
{
  // setup perspective matrix...
  GLfloat aspect = (GLfloat) w / (GLfloat) h;
  glViewport(0, 0, w, h);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->LoadIdentity();
  matrix->Perspective(FOV, aspect, 0.01, 50.0);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);
}
//take screenshots in the timer interval
void screenshotTimer(int value)
{
  switch(value){
    case 0:
      if(!screenShotFlag) break;
      char fileName[40];
      sprintf(fileName, "screenshots/%03d.jpg", screenshotCounter);
      saveScreenshot(fileName);
      screenshotCounter++;
      if(screenshotCounter == 1000)
        screenShotFlag = false;
      break;
    default:
      break;
  }
  glutTimerFunc(timer, screenshotTimer, 0);
}

//load the .sp files from track.txt
int loadSplines(char * argv)
{
  char * cName = (char *) malloc(128 * sizeof(char));
  FILE * fileList;
  FILE * fileSpline;
  int iType, i = 0, j, iLength;

  // load the track file
  fileList = fopen(argv, "r");
  if (fileList == NULL)
  {
    printf ("can't open file\n");
    exit(1);
  }

  // stores the number of splines in a global variable
  fscanf(fileList, "%d", &numSplines);

  splines = (Spline*) malloc(numSplines * sizeof(Spline));

  // reads through the spline files
  for (j = 0; j < numSplines; j++)
  {
    i = 0;
    fscanf(fileList, "%s", cName);
    fileSpline = fopen(cName, "r");

    if (fileSpline == NULL)
    {
      printf ("can't open file\n");
      exit(1);
    }

    // gets length for spline file
    fscanf(fileSpline, "%d %d", &iLength, &iType);

    // allocate memory for all the points
    splines[j].points = (Point *)malloc(iLength * sizeof(Point));
    splines[j].numControlPoints = iLength;

    // saves the data to the struct
    while (fscanf(fileSpline, "%lf %lf %lf",
	   &splines[j].points[i].x,
	   &splines[j].points[i].y,
	   &splines[j].points[i].z) != EOF)
    {
      i++;
    }
  }

  free(cName);

  return 0;
}

void mouseMotionDragFunc(int x, int y)
{
  // mouse has moved and one of the mouse buttons is pressed (dragging)

  // the change in mouse position since the last invocation of this function
  int mousePosDelta[2] = { x - mousePos[0], y - mousePos[1] };

  switch (controlState)
  {
    // translate the landscape
    case TRANSLATE:
      if (leftMouseButton)
      {
        // control x,y translation via the left mouse button
        landTranslate[0] += mousePosDelta[0] * 0.01f;
        landTranslate[1] -= mousePosDelta[1] * 0.01f;
      }
      if (middleMouseButton)
      {
        // control z translation via the middle mouse button
        landTranslate[2] += mousePosDelta[1] * 0.01f;
      }
      break;

    // rotate the landscape
    case ROTATE:
      if (leftMouseButton)
      {
        // control x,y rotation via the left mouse button
        landRotate[0] += mousePosDelta[1];
        landRotate[1] += mousePosDelta[0];
      }
      if (middleMouseButton)
      {
        // control z rotation via the middle mouse button
        landRotate[2] += mousePosDelta[1];
      }
      break;

    // scale the landscape
    case SCALE:
      if (leftMouseButton)
      {
        // control x,y scaling via the left mouse button
        landScale[0] *= 1.0f + mousePosDelta[0] * 0.01f;
        landScale[1] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      if (middleMouseButton)
      {
        // control z scaling via the middle mouse button
        landScale[2] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      break;
  }

  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseMotionFunc(int x, int y)
{
  // mouse has moved
  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseButtonFunc(int button, int state, int x, int y)
{
  // a mouse button has has been pressed or depressed

  // keep track of the mouse button state, in leftMouseButton, middleMouseButton, rightMouseButton variables
  switch (button)
  {
    case GLUT_LEFT_BUTTON:
      leftMouseButton = (state == GLUT_DOWN);
    break;

    case GLUT_MIDDLE_BUTTON:
      middleMouseButton = (state == GLUT_DOWN);
    break;

    case GLUT_RIGHT_BUTTON:
      rightMouseButton = (state == GLUT_DOWN);
    break;
  }

  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void keyboardFunc(unsigned char key, int x, int y)
{
  switch (key)
  {
    case 27: // ESC key
      exit(0); // exit the program
    break;

    case ' ':
      cout << "You pressed the spacebar." << endl;
    break;

    case 'x':
      // take a screenshot
      saveScreenshot("screenshot.jpg");
    break;
  }
}

int main(int argc, char *argv[])
{
  cout << "Initializing GLUT..." << endl;
  glutInit(&argc,argv);

  cout << "Initializing OpenGL..." << endl;

  #ifdef __APPLE__
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
  #else
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
  #endif

  glutInitWindowSize(windowWidth, windowHeight);
  glutInitWindowPosition(0, 0);
  glutCreateWindow(windowTitle);

  cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
  cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << endl;
  cout << "Shading Language Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

  // tells glut to use a particular display function to redraw
  glutDisplayFunc(display);
  // perform animation inside idleFunc
  glutIdleFunc(idleFunc);
  // callback for mouse drags
  glutMotionFunc(mouseMotionDragFunc);
  // callback for idle mouse movement
  glutPassiveMotionFunc(mouseMotionFunc);
  // callback for mouse button changes
  glutMouseFunc(mouseButtonFunc);
  // callback for resizing the window
  glutReshapeFunc(reshapeFunc);
  // callback for pressing the keys on the keyboard
  glutKeyboardFunc(keyboardFunc);
  // callback for a timer to save screenshots
  //glutTimerFunc(timer, screenshotTimer, 0);

  // init glew
  #ifdef __APPLE__
    // nothing is needed on Apple
  #else
    // Windows, Linux
    GLint result = glewInit();
    if (result != GLEW_OK)
    {
      cout << "error: " << glewGetErrorString(result) << endl;
      exit(EXIT_FAILURE);
    }
  #endif

  if (argc<2)
  {
    printf ("usage: %s <trackfile>\n", argv[0]);
    exit(0);
  }

  // load the splines from the provided filename
  loadSplines(argv[1]);

  printf("Loaded %d spline(s).\n", numSplines);
  for(int i=0; i<numSplines; i++)
    printf("Num control points in spline %d: %d.\n", i, splines[i].numControlPoints);

  // do initialization
  initScene(argc, argv);

  // sink forever into the glut loop
  glutMainLoop();
}
