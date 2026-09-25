// Author: Imanol Munoz-Pandiella 2023 based on Marc Comino 2020

#include <glwidget.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <QImage>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "./mesh_io.h"
#include "./triangle_mesh.h"

#include <glm/mat4x4.hpp>
#include <random>

namespace {

const double kFieldOfView = 60;
const double kZNear = 0.0001;
const double kZFar = 20;

const std::vector<std::vector<std::string>> kShaderFiles = {
                {"../shaders/phong.vert",        "../shaders/phong.frag"},
                {"../shaders/texMap.vert",       "../shaders/texMap.frag"},
                {"../shaders/reflection.vert",   "../shaders/reflection.frag"},
                {"../shaders/pbs.vert",          "../shaders/pbs.frag"},
                {"../shaders/ibl-pbs.vert",      "../shaders/ibl-pbs.frag"},
                {"../shaders/ssaoGeom.vert",     "../shaders/ssaoGeom.frag"},
                {"../shaders/sky.vert",          "../shaders/sky.frag"}};//sky needs to be the last one

const int kVertexAttributeIdx = 0;
const int kNormalAttributeIdx = 1;
const int kTexCoordAttributeIdx = 2;


bool ReadFile(const std::string filename, std::string *shader_source) {
  std::ifstream infile(filename.c_str());

  if (!infile.is_open() || !infile.good()) {
    std::cerr << "Error " + filename + " not found." << std::endl;
    return false;
  }

  std::stringstream stream;
  stream << infile.rdbuf();
  infile.close();

  *shader_source = stream.str();
  return true;
}

bool LoadImage(const std::string &path, GLuint cube_map_pos) {
  QImage image;
  bool res = image.load(path.c_str());
 
  if (res) {
    QImage gl_image = image.mirrored();
    glTexImage2D(cube_map_pos, 0, GL_RGBA, image.width(), image.height(), 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, image.bits());
    
  }
  return res;
}

bool LoadCubeMap(const QString &dir) {
  std::string path = dir.toUtf8().constData();
  bool res = LoadImage(path + "/right.png", GL_TEXTURE_CUBE_MAP_POSITIVE_X);
  res = res && LoadImage(path + "/left.png", GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
  res = res && LoadImage(path + "/top.png", GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
  res = res && LoadImage(path + "/bottom.png", GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
  res = res && LoadImage(path + "/back.png", GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
  res = res && LoadImage(path + "/front.png", GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);

  if (res) {
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
  }

  return res;
}

bool LoadProgram(const std::string &vertex, const std::string &fragment,
                 QOpenGLShaderProgram *program) {
  std::string vertex_shader, fragment_shader;
  bool res =
      ReadFile(vertex, &vertex_shader) && ReadFile(fragment, &fragment_shader);

  if (res) {
    program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                     vertex_shader.c_str());
    program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                     fragment_shader.c_str());
    program->bindAttributeLocation("vertex", kVertexAttributeIdx);
    program->bindAttributeLocation("normal", kNormalAttributeIdx);
    program->bindAttributeLocation("texCoord", kTexCoordAttributeIdx);
    program->link();
  }

  return res;
}

glm::mat4x4 generate_direction(int n){
  glm::mat4x4 direction_matrix = glm::mat4x4(1.f);
  switch (n) {
    case 0:

      break;
    case 1:
      direction_matrix = glm::rotate(direction_matrix, glm::radians(90.0f), glm::vec3(0.0,1.0,0.0));
      break;
    case 2:
      direction_matrix = glm::rotate(direction_matrix, glm::radians(180.0f), glm::vec3(0.0,1.0,0.0));
      break;
    case 3:
      direction_matrix = glm::rotate(direction_matrix, glm::radians(270.0f), glm::vec3(0.0,1.0,0.0));
      break;
    case 4:    
      direction_matrix = glm::rotate(direction_matrix, glm::radians(90.0f), glm::vec3(1.0,0.0,0.0));
      direction_matrix = glm::rotate(direction_matrix, glm::radians(180.0f), glm::vec3(0.0,0.0,1.0));
      break;
    case 5:
      direction_matrix = glm::rotate(direction_matrix, glm::radians(-90.0f), glm::vec3(1.0,0.0,0.0));
      direction_matrix = glm::rotate(direction_matrix, glm::radians(180.0f), glm::vec3(0.0,0.0,1.0));
      break;
    default:
      break;
  }
  return direction_matrix;
}
}  // namespace

GLWidget::GLWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      initialized_(false),
      width_(0.0),
      height_(0.0),
      currentShader_(0),
      currentTexture_(0),
      fresnel_(0.2, 0.2, 0.2),
      skyVisible_(true),
      metalness_(0),
      roughness_(0)
        {
  setFocusPolicy(Qt::StrongFocus);
}

GLWidget::~GLWidget() {
  if (initialized_) {
    glDeleteTextures(1, &specular_map_);
    glDeleteTextures(1, &diffuse_map_);
  }
}

bool GLWidget::LoadModel(const QString &filename) {
  std::string file = filename.toUtf8().constData();
  size_t pos = file.find_last_of(".");
  std::string type = file.substr(pos + 1);
   
  std::unique_ptr<data_representation::TriangleMesh> mesh =
      std::make_unique<data_representation::TriangleMesh>();

  bool res = false;
  if (type.compare("ply") == 0) {
    res = data_representation::ReadFromPly(file, mesh.get());
  } else if (type.compare("obj") == 0) {
    res = data_representation::ReadFromObj(file, mesh.get());
  } else if(type.compare("null") == 0) {
    res = data_representation::CreateSphere(mesh.get());
  }

  if (res) {
    //mesh_.release();
    mesh_.reset(mesh.release());
    camera_.UpdateModel(mesh_->min_, mesh_->max_);
    //mesh_->computeNormals();

    int nVertices = mesh_->vertices_.size();
    int nFaces =  mesh_->faces_.size();
    /*for (int i = 0; i < nFaces; i += 3) {
      std::cout << mesh_ -> faces_[i] << " " << mesh_ ->faces_[i + 1]<< " " << mesh_ ->faces_[i + 2]<< std::endl;
    }*/

    // TODO(students): Create / Initialize buffers.
    //MESH: You need to create 1 VAO and 4 VBO
    
    glBindVertexArray(VAO);
    //mesh_->vertices -> attrib location 0
    glBindBuffer(GL_ARRAY_BUFFER, VBO_v);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*nVertices, &(mesh_->vertices_[0]), GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);
    glEnableVertexAttribArray(0);
    //mesh_->normals -> attrib location 1
    glBindBuffer(GL_ARRAY_BUFFER, VBO_n);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*nVertices, &(mesh_->normals_[0]), GL_STATIC_DRAW);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,0,0);
    glEnableVertexAttribArray(1);
    //mesh_->texCoords -> attrib location 2
    glBindBuffer(GL_ARRAY_BUFFER, VBO_tc);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*mesh_->texCoords_.size(), &mesh_->texCoords_[0], GL_STATIC_DRAW);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,0,0);
    glEnableVertexAttribArray(2);
    //mesh_->faces -> elements
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBO_i);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int)*nFaces, &(mesh_->faces_[0]), GL_STATIC_DRAW);
    glBindVertexArray(0);

    skyVertices_ = {
      // positions
      1.0f,1.0f,1.0f,
      1.0f,1.0f,-1.0f,
      1.0f,-1.0f,1.0f,
      1.0f,-1.0f,-1.0f,
      -1.0f,1.0f,1.0f,
      -1.0f,1.0f,-1.0f,
      -1.0f,-1.0f,1.0f,
      -1.0f,-1.0f,-1.0f,

    };

    skyFaces_ = std::vector<int>();
    skyFaces_.push_back(5);
    skyFaces_.push_back(7);
    skyFaces_.push_back(3);

    skyFaces_.push_back(3);
    skyFaces_.push_back(1);
    skyFaces_.push_back(5);

    skyFaces_.push_back(6);
    skyFaces_.push_back(7);
    skyFaces_.push_back(5);

    skyFaces_.push_back(5);
    skyFaces_.push_back(4);
    skyFaces_.push_back(6);

    skyFaces_.push_back(3);
    skyFaces_.push_back(2);
    skyFaces_.push_back(0);

    skyFaces_.push_back(0);
    skyFaces_.push_back(1);
    skyFaces_.push_back(3);

    skyFaces_.push_back(6);
    skyFaces_.push_back(4);
    skyFaces_.push_back(0);

    skyFaces_.push_back(0);
    skyFaces_.push_back(2);
    skyFaces_.push_back(6);

    skyFaces_.push_back(5);
    skyFaces_.push_back(1);
    skyFaces_.push_back(0);

    skyFaces_.push_back(0);
    skyFaces_.push_back(4);
    skyFaces_.push_back(5);

    skyFaces_.push_back(7);
    skyFaces_.push_back(6);
    skyFaces_.push_back(3);

    skyFaces_.push_back(3);
    skyFaces_.push_back(6);
    skyFaces_.push_back(2);


    //SKY BOX: You need to create 1 VAO and 2 VBO:
    
    glBindVertexArray(VAO_sky);
    // vertices -> attrib location 0
    glBindBuffer(GL_ARRAY_BUFFER, VBO_v_sky);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*skyVertices_.size(), &skyVertices_[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);
    glEnableVertexAttribArray(0);
    //faces -> elements
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBO_i_sky);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(float)*skyFaces_.size(), &skyFaces_[0], GL_STATIC_DRAW);
    glBindVertexArray(0);

    std::vector<float> quadVertices_ = {
      -1.f, -1.f, -1.f,
      -1.f, 1.f, -1.0f,
      1.f, -1.f, -1.0f,
      1.f, 1.f, -1.0f
    };

    std::vector<int> quadIndices_ = {
      0,1,2,2,1,3
    };



    glBindVertexArray(VAO_quad);
    // vertices -> attrib location 0
    glBindBuffer(GL_ARRAY_BUFFER, VBO_v_quad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*quadVertices_.size(), &quadVertices_[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);
    glEnableVertexAttribArray(0);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBO_i_quad);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(float)*quadIndices_.size(), &quadIndices_[0], GL_STATIC_DRAW);
    glBindVertexArray(0);

    
    emit SetFaces(QString(std::to_string(mesh_->faces_.size() / 3).c_str()));
    emit SetVertices(
        QString(std::to_string(mesh_->vertices_.size() / 3).c_str()));
    return true;
  }

  update();
  return false;
}

bool GLWidget::LoadSpecularMap(const QString &dir) {
  glBindTexture(GL_TEXTURE_CUBE_MAP, specular_map_);
  bool res = LoadCubeMap(dir);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  generate_prefilter = true;
  update();
  return res;
}

bool GLWidget::LoadDiffuseMap(const QString &dir) {
  glBindTexture(GL_TEXTURE_CUBE_MAP, diffuse_map_);
  bool res = LoadCubeMap(dir);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  update();
  return res;
}

bool GLWidget::LoadColorMap(const QString &filename)
{
    //TODO Students
    //Configure the texture with identifier color_map_. Take advantage of LoadImage("path", GL_TEXTURE_2D).
    //Remember to configure the texture parameters.
    glBindTexture(GL_TEXTURE_2D, color_map_);
    bool res = LoadImage(filename.toStdString(), GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    //TODO END
    update();
    return res;

}

bool GLWidget::LoadRoughnessMap(const QString &filename)
{
    //TODO Students
    //Configure the texture with identifier roughness_map_. Take advantage of LoadImage("path", GL_TEXTURE_2D)
    //Remember to configure the texture parameters.
    glBindTexture(GL_TEXTURE_2D, roughness_map_);
    bool res = LoadImage(filename.toStdString(), GL_TEXTURE_2D);
 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    //TODO END
    update();
    return res;
}

bool GLWidget::LoadMetalnessMap(const QString &filename)
{
    //TODO Students
    //Configure the texture with identifier metalness_map_. Take advantage of LoadImage("path", GL_TEXTURE_2D)
    //Remember to configure the texture parameters.
    glBindTexture(GL_TEXTURE_2D, metalness_map_);
    bool res = LoadImage(filename.toStdString(), GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    //TODO END
    update();
    return res;
}

void GLWidget::GenerateDiffuseIrradiance() 
{
  generate_diffuse = true;
}

void GLWidget::GenerateSpecularIrradiance() {}

void GLWidget::InitializeSSAOBuffers() {
  glDeleteFramebuffers(1,&ssaoFBO);
  glDeleteFramebuffers(1,&imrpovedAO_FBO);
  glDeleteTextures(1,&imrpovedAO_Text);
  glDeleteTextures(1,&ssaoAlbedo);
  glDeleteTextures(1,&ssaoNormal);
  glDeleteTextures(1,&ssaoDepth);


  glGenFramebuffers(1, &ssaoFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);

  glGenTextures(1, &ssaoAlbedo);
  glBindTexture(GL_TEXTURE_2D, ssaoAlbedo);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoAlbedo, 0);

  glGenTextures(1, &ssaoNormal);
  glBindTexture(GL_TEXTURE_2D, ssaoNormal);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F,  width_, height_, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, ssaoNormal, 0);

  glGenTextures(1, &ssaoDepth);
  glBindTexture(GL_TEXTURE_2D, ssaoDepth);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,  width_, height_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, ssaoDepth, 0);

  GLuint textureAttatchments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
  glDrawBuffers(2, textureAttatchments);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);  
  glBindTexture(GL_TEXTURE_2D,0);
  
  
  glGenFramebuffers(1, &imrpovedAO_FBO);
  glBindFramebuffer(GL_FRAMEBUFFER, imrpovedAO_FBO);

  glGenTextures(1, &imrpovedAO_Text);
  glBindTexture(GL_TEXTURE_2D, imrpovedAO_Text);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, imrpovedAO_Text, 0);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
      std::cerr << "Framebuffer not complete. Status: 0x" << std::hex << status << std::endl;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void GLWidget::initializeGL ()
{
  // Cal inicialitzar l'ús de les funcions d'OpenGL
  initializeOpenGLFunctions();

  //initializing opengl state
  glEnable(GL_NORMALIZE);
  glDisable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glEnable(GL_DEPTH_TEST);
  //generating needed textures
  glGenTextures(1, &specular_map_);
  glGenTextures(1, &diffuse_map_);
  glGenTextures(1, &color_map_);
  glGenTextures(1, &roughness_map_);
  glGenTextures(1, &metalness_map_);
  
  fbos = new GLuint[6];
  fboColor = new GLuint[6];
  fboDepthStencil = new GLuint[6];
  
  glGenFramebuffers(1, &prefilterfbo);
  glGenRenderbuffers(1,&rboprefilter);
  glGenTextures(1, &prefilter_map);

  glGenFramebuffers(6,fbos);
  glGenTextures(6,fboColor);
  glGenRenderbuffers(6, fboDepthStencil);
      
  for (int i = 0; i < 6; ++i) {
    glBindFramebuffer(GL_FRAMEBUFFER,fbos[i]);
    
    glBindTexture(GL_TEXTURE_2D, fboColor[i]);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_2D,0);
    
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboColor[i], 0);


    glBindRenderbuffer(GL_RENDERBUFFER, fboDepthStencil[i]); 
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 256, 256);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fboDepthStencil[i]);

  }
    
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  std::uniform_real_distribution<float> randomFloat(0.0,1.0);
  std::default_random_engine gen;
  std::vector<glm::vec3> noise(1024);
  for (unsigned int i = 0; i < 1024; i++)
  {
      noise[i] = glm::vec3(
          randomFloat(gen) * 2.0 - 1.0, 
          randomFloat(gen) * 2.0 - 1.0, 
          0.0f); 
  } 

  glGenTextures(1, &noiseText);
  glBindTexture(GL_TEXTURE_2D, noiseText);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 32, 32, 0, GL_RGB, GL_FLOAT, &noise[0]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glBindTexture(GL_TEXTURE_2D, 0);
  //create shader programs
  programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//phong
  programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//texture mapping
  programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//reflection
  programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//simple pbs
  programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//ibl pbs
  programs_.push_back(std::make_unique<QOpenGLShaderProgram>()); //SSAO Geom
  programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//sky

  secondStepPrograms_.push_back(std::make_unique<QOpenGLShaderProgram>()); //TwoStep Renderer
  secondStepPrograms_.push_back(std::make_unique<QOpenGLShaderProgram>()); //Basic AO estimator
  secondStepPrograms_.push_back(std::make_unique<QOpenGLShaderProgram>()); //Improved AO estimator 
  secondStepPrograms_.push_back(std::make_unique<QOpenGLShaderProgram>()); //Blur step 

  //load vertex and fragment shader files
  bool res =   LoadProgram(kShaderFiles[0][0],   kShaderFiles[0][1],    programs_[0].get());
  res = res && LoadProgram(kShaderFiles[1][0],   kShaderFiles[1][1],    programs_[1].get());
  res = res && LoadProgram(kShaderFiles[2][0],   kShaderFiles[2][1],    programs_[2].get());
  res = res && LoadProgram(kShaderFiles[3][0],   kShaderFiles[3][1],    programs_[3].get());
  res = res && LoadProgram(kShaderFiles[4][0],   kShaderFiles[4][1],    programs_[4].get());
  res = res && LoadProgram(kShaderFiles[5][0],   kShaderFiles[5][1],    programs_[5].get());
  res = res && LoadProgram(kShaderFiles[6][0],   kShaderFiles[6][1],    programs_[6].get());
  res = res && LoadProgram("../shaders/diffuseGen.vert", "../shaders/diffuseGen.frag", &diffuse_generator);
  res = res && LoadProgram("../shaders/specularGen.vert", "../shaders/specularGen.frag", &specular_generator);
  res = res && LoadProgram("../shaders/ssao2Step.vert", "../shaders/ssao2Step.frag", secondStepPrograms_[0].get());
  res = res && LoadProgram("../shaders/basic_hbao.vert", "../shaders/basic_hbao.frag", secondStepPrograms_[1].get());
  res = res && LoadProgram("../shaders/improved_hbao.vert", "../shaders/improved_hbao.frag", secondStepPrograms_[2].get());
  res = res && LoadProgram("../shaders/blur.vert", "../shaders/blur.frag", secondStepPrograms_[3].get());
  
  

  if (!res) exit(0);

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1,&VBO_i);
  glGenBuffers(1,&VBO_n);
  glGenBuffers(1,&VBO_tc);
  glGenBuffers(1,&VBO_v);

  glGenVertexArrays(1, &VAO_sky);
  glGenBuffers(1, &VBO_v_sky);
  glGenBuffers(1, &VBO_i_sky);

  glGenVertexArrays(1, &VAO_quad);
  glGenBuffers(1, &VBO_v_quad);
  glGenBuffers(1, &VBO_i_quad);
  LoadModel(".null");//create an sphere

  initialized_ = true;
}


void GLWidget::resizeGL (int w, int h)
{
    if (h == 0) h = 1;
    width_ = w;
    height_ = h;

    camera_.SetViewport(0, 0, w, h);
    camera_.SetProjection(kFieldOfView, kZNear, kZFar);
    InitializeSSAOBuffers();
}

void GLWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    camera_.StartRotating(event->x(), event->y());
  }
  if (event->button() == Qt::RightButton) {
    camera_.StartZooming(event->x(), event->y());
  }
  update();
}

void GLWidget::mouseMoveEvent(QMouseEvent *event) {
  camera_.SetRotationX(event->y());
  camera_.SetRotationY(event->x());
  camera_.SafeZoom(event->y());
  update();
}

void GLWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    camera_.StopRotating(event->x(), event->y());
  }
  if (event->button() == Qt::RightButton) {
    camera_.StopZooming(event->x(), event->y());
  }
  update();
}

void GLWidget::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Up) camera_.Zoom(-1);
  if (event->key() == Qt::Key_Down) camera_.Zoom(1);

  if (event->key() == Qt::Key_Left) camera_.Rotate(-1);
  if (event->key() == Qt::Key_Right) camera_.Rotate(1);

  if (event->key() == Qt::Key_W) camera_.Zoom(-1);
  if (event->key() == Qt::Key_S) camera_.Zoom(1);

  if (event->key() == Qt::Key_A) camera_.Rotate(-1);
  if (event->key() == Qt::Key_D) camera_.Rotate(1);

  if (event->key() == Qt::Key_R) {
      for(auto i = 0; i < programs_.size(); ++i) {
          programs_[i].reset();
          programs_[i] = std::make_unique<QOpenGLShaderProgram>();
          LoadProgram(kShaderFiles[i][0], kShaderFiles[i][1], programs_[i].get());
      }
  }

  update();
}


void GLWidget::generatePrefilteredMaps(){

  
  glBindTexture(GL_TEXTURE_CUBE_MAP, prefilter_map);
  for (unsigned int i = 0; i < 6; ++i)
  {
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 256, 256, 0, GL_RGB, GL_FLOAT, nullptr);
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);


  specular_generator.bind();
  GLuint specular_map_location = specular_generator.uniformLocation("specular_map");
  GLuint roughness_location = specular_generator.uniformLocation("roughness");
  GLuint direction_location = specular_generator.uniformLocation("direction");

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, specular_map_);
  glUniform1i(specular_map_location, 0);


  glBindFramebuffer(GL_FRAMEBUFFER, prefilterfbo);
  unsigned int maxMipLevels = 6;
  int directions[6] = {3,1,4,5,2,0};
  for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
  {
      // reisze framebuffer according to mip-level size.
      unsigned int mipWidth  = 256 / std::pow(2, mip);
      unsigned int mipHeight = 256 / std::pow(2, mip);
      glBindRenderbuffer(GL_RENDERBUFFER, rboprefilter);
      glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
      glViewport(0, 0, mipWidth, mipHeight);

      float rough = (float)mip / (float)(maxMipLevels - 1);
      glUniform1f(roughness_location, rough);

      for (unsigned int i = 0; i < 6; ++i)
      {
          glm::mat4x4 direction_matrix = generate_direction(directions[i]);
          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilter_map, mip);

          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
          
          glUniformMatrix4fv(direction_location, 1, GL_FALSE, &direction_matrix[0][0]);

          glBindVertexArray(VAO_quad);
          glDrawElements(GL_TRIANGLES,6, GL_UNSIGNED_INT, (GLvoid*)0);
          glBindVertexArray(0);
          
          
      }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  update();
}

void GLWidget::paintDiffuseIrradiance(int face) 
{
  glm::mat4x4 direction_matrix = generate_direction(face);
  
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
  GLint specular_map_location, direction_location;
  diffuse_generator.bind();

  specular_map_location   = diffuse_generator.uniformLocation("specular_map");
  direction_location   = diffuse_generator.uniformLocation("direction");
  
  glUniformMatrix4fv(direction_location, 1, GL_FALSE, &direction_matrix[0][0]); 

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, specular_map_);
  glUniform1i(specular_map_location, 0);

  glBindVertexArray(VAO_quad);
  glDrawElements(GL_TRIANGLES,6, GL_UNSIGNED_INT, (GLvoid*)0);
  glBindVertexArray(0);

}

void GLWidget::renderSecondStep() {

  secondStepPrograms_[currentTwoStepShader_] -> bind();
  GLuint current_text_location, depth_location, normals_location,directions_location, 
  radius_location, samples_location, far_location, near_location, fov_location, 
  aspect_ratio_loaction, noise_location, albedo_location;

  current_text_location     = secondStepPrograms_[currentTwoStepShader_] -> uniformLocation("current_texture");
  depth_location            = secondStepPrograms_[currentTwoStepShader_] -> uniformLocation("depth");
  normals_location          = secondStepPrograms_[currentTwoStepShader_] -> uniformLocation("normals");
  radius_location           = secondStepPrograms_[currentTwoStepShader_] -> uniformLocation("radius");
  samples_location          = secondStepPrograms_[currentTwoStepShader_] -> uniformLocation("nSamples");
  directions_location       = secondStepPrograms_[currentTwoStepShader_] -> uniformLocation("nDirections");
  far_location              = secondStepPrograms_[currentTwoStepShader_] -> uniformLocation("far");
  near_location             = secondStepPrograms_[currentTwoStepShader_] -> uniformLocation("near");
  fov_location              = secondStepPrograms_[currentTwoStepShader_] -> uniformLocation("fov");
  aspect_ratio_loaction     = secondStepPrograms_[currentTwoStepShader_] -> uniformLocation("aspect_ratio");
  noise_location            = secondStepPrograms_[currentTwoStepShader_] -> uniformLocation("noise");
  albedo_location           = secondStepPrograms_[currentTwoStepShader_] -> uniformLocation("albedo");

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, ssaoAlbedo);
  glUniform1i(albedo_location, 0);  
  
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, ssaoNormal);
  glUniform1i(normals_location, 1);  

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, ssaoDepth);
  glUniform1i(depth_location, 2);
  
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, noiseText);
  glUniform1i(noise_location, 3);

  glUniform1i(current_text_location, twoStepMode);
  glUniform1i(samples_location, nSamples);
  glUniform1i(directions_location, nDirections);
  glUniform1f(radius_location, radius);
  glUniform1f(far_location, kZFar);
  if(currentTwoStepShader_ == 0 && twoStepMode != 2) glUniform1f(near_location, -1.0); 
  else glUniform1f(near_location, kZNear);
  glUniform1f(fov_location, kFieldOfView);
  glUniform1f(aspect_ratio_loaction, float(width_)/float(height_));

  glBindVertexArray(VAO_quad);
  glDrawElements(GL_TRIANGLES,6, GL_UNSIGNED_INT, (GLvoid*)0);
  glBindVertexArray(0);
 
}

void GLWidget::renderQuad() {
  programs_[1] -> bind();
  GLuint current_text_location;

  current_text_location     = programs_[currentShader_]->uniformLocation("current_texture");

  glActiveTexture(GL_TEXTURE6);
  glBindTexture(GL_TEXTURE_2D, ssaoNormal);
  glUniform1i(current_text_location, 6);

  glBindVertexArray(VAO_quad);
  glDrawElements(GL_TRIANGLES,6, GL_UNSIGNED_INT, (GLvoid*)0);
  glBindVertexArray(0);
  
}
void GLWidget::renderPBR() {
  if (initialized_) {
    camera_.SetViewport();

    glm::mat4x4 projection = camera_.SetProjection();
    glm::mat4x4 view = camera_.SetView();
    glm::mat4x4 model = camera_.SetModel();

    //compute normal matrix
    glm::mat4x4 t = view * model;
    glm::mat3x3 normal;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            normal[i][j] = t[i][j];
     normal = glm::transpose(glm::inverse(normal));

    if (mesh_ != nullptr) {
        GLint projection_location, view_location, model_location,
        normal_matrix_location, specular_map_location, diffuse_map_location,
        fresnel_location, color_map_location, roughness_map_location, metalness_map_location,
        current_text_location, light_location, roughness_location, metalness_location, prefilter_map_location;

        //SKY-----------------------------------------------------------------------------------------
        if(skyVisible_) {
          //model = camera_.SetIdentity();
          
          programs_[programs_.size()-1]->bind();

          projection_location     = programs_[programs_.size()-1]->uniformLocation("projection");
          view_location           = programs_[programs_.size()-1]->uniformLocation("view");
          model_location          = programs_[programs_.size()-1]->uniformLocation("model");
          normal_matrix_location  = programs_[programs_.size()-1]->uniformLocation("normal_matrix");
          specular_map_location   = programs_[programs_.size()-1]->uniformLocation("specular_map");
          

          glm::mat4x4 skyView = glm::mat4x4(glm::mat3x3(view));
          glUniformMatrix4fv(projection_location, 1, GL_FALSE, &projection[0][0]);
          glUniformMatrix4fv(view_location, 1, GL_FALSE, &skyView[0][0]);
          glUniformMatrix4fv(model_location, 1, GL_FALSE, &model[0][0]);
          glUniformMatrix3fv(normal_matrix_location, 1, GL_FALSE, &normal[0][0]);

          glDepthMask(GL_FALSE);
          glActiveTexture(GL_TEXTURE0);
          glBindTexture(GL_TEXTURE_CUBE_MAP, specular_map_);
          glUniform1i(specular_map_location, 0);

          // TODO(students): implement the draw call of the sky box
          glBindVertexArray(VAO_sky);
          glDrawElements(GL_TRIANGLES,skyFaces_.size(), GL_UNSIGNED_INT, (GLvoid*)0);
          glBindVertexArray(0);
          glDepthMask(GL_TRUE);
          // TODO END.
        }
        //MESH-----------------------------------------------------------------------------------------
        //general shader setting
  
        programs_[currentShader_]->bind();

        projection_location       = programs_[currentShader_]->uniformLocation("projection");
        view_location             = programs_[currentShader_]->uniformLocation("view");
        model_location            = programs_[currentShader_]->uniformLocation("model");
        normal_matrix_location    = programs_[currentShader_]->uniformLocation("normal_matrix");
        specular_map_location     = programs_[currentShader_]->uniformLocation("specular_map");
        diffuse_map_location      = programs_[currentShader_]->uniformLocation("diffuse_map");
        color_map_location        = programs_[currentShader_]->uniformLocation("color_map");
        roughness_map_location    = programs_[currentShader_]->uniformLocation("roughness_map");
        metalness_map_location    = programs_[currentShader_]->uniformLocation("metalness_map");
        current_text_location     = programs_[currentShader_]->uniformLocation("current_texture");
        fresnel_location          = programs_[currentShader_]->uniformLocation("fresnel");
        light_location            = programs_[currentShader_]->uniformLocation("light");
        roughness_location        = programs_[currentShader_]->uniformLocation("roughness");
        metalness_location        = programs_[currentShader_]->uniformLocation("metalness");
        prefilter_map_location    = programs_[currentShader_] -> uniformLocation("prefilter_map");

        glUniformMatrix4fv(projection_location, 1, GL_FALSE, &projection[0][0]);
        glUniformMatrix4fv(view_location, 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(model_location, 1, GL_FALSE, &model[0][0]);
        glUniformMatrix3fv(normal_matrix_location, 1, GL_FALSE, &normal[0][0]);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, specular_map_);
        glUniform1i(specular_map_location, 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, diffuse_map_);
        glUniform1i(diffuse_map_location, 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, prefilter_map);
        glUniform1i(prefilter_map_location, 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, color_map_);
        glUniform1i(color_map_location, 3);
        
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, roughness_map_);
        glUniform1i(roughness_map_location, 4);
        
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, metalness_map_);
        glUniform1i(metalness_map_location, 5);
        
        glUniform1i(current_text_location, 3 + currentTexture_);
        glUniform3f(fresnel_location, fresnel_[0], fresnel_[1], fresnel_[2]);
        glUniform3f(light_location, 10, 0, 0);
        glUniform1f(roughness_location, roughness_);
        glUniform1f(metalness_location, metalness_);


        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES,mesh_->faces_.size(), GL_UNSIGNED_INT, (GLvoid*)0);
        glBindVertexArray(0);

        // TODO END.


        
    }
  }
}

void GLWidget::renderSSAO() {
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  int auxShader = currentShader_;
  if(!AOAlbedo) currentShader_ = 5;
  glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  renderPBR();
  currentShader_ = auxShader;

  if (!imrpovedAO) glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
  else glBindFramebuffer(GL_FRAMEBUFFER, imrpovedAO_FBO);
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  renderSecondStep();

  if (imrpovedAO) {
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderBlur();
  }
} 

void GLWidget::renderBlur() {
  secondStepPrograms_[secondStepPrograms_.size()-1]->bind();
  GLuint scene_text_location, albedo_location;
  scene_text_location       = secondStepPrograms_[secondStepPrograms_.size()-1] -> uniformLocation("scene_text");
  albedo_location           = secondStepPrograms_[currentTwoStepShader_] -> uniformLocation("albedo");

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, imrpovedAO_Text);
  glUniform1i(scene_text_location, 1);  

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, ssaoAlbedo);
  glUniform1i(albedo_location, 0);

  glBindVertexArray(VAO_quad);
  glDrawElements(GL_TRIANGLES,6, GL_UNSIGNED_INT, (GLvoid*)0);
  glBindVertexArray(0);
}

void GLWidget::paintGL ()
{

  
    if (generate_diffuse) {
      int w,h;
      w = width_;
      h = height_;
      resizeGL(256,256);

      unsigned char* buffer = new unsigned char[256 * 256 * 4];
        
      QString names[6] = {"front", "left", "back", "right", "top", "bottom"};
      
      //glDepthMask(GL_FALSE);
      for (int i = 0; i < 6; ++i) {
        
        glBindFramebuffer(GL_FRAMEBUFFER, fbos[i]);
    
        paintDiffuseIrradiance(i);

          
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(0,0,256,256, GL_BGRA, GL_UNSIGNED_BYTE, buffer);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
          
        QString savePath = "./generated/" + names[i] + ".png"; 
        QImage img = QImage(buffer, 256, 256, QImage::Format_RGB32);
        img.save(savePath, "PNG");
      }  
      //glDepthMask(GL_TRUE);
      generate_diffuse = false;
      resizeGL(w,h);
      glBindFramebuffer(GL_FRAMEBUFFER,0);
      
    }

    if (generate_prefilter) {

      generatePrefilteredMaps();
      generate_prefilter = false;
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
  
    if (twoStep) {

      if (initialized_) renderSSAO();
    }
    
    else {
      glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      renderPBR();
      
    }
}

void GLWidget::SetReflection(bool set) {
    if(set) currentShader_ = 2;
    update();
}

void GLWidget::SetPBS(bool set) {
    if(set) currentShader_ = 3;
    update();
}

void GLWidget::SetIBLPBS(bool set) {
    if(set) currentShader_ = 4;
    update();
}

void GLWidget::SetPhong(bool set)
{
    if(set) currentShader_ = 0;
    update();
}

void GLWidget::SetTexMap(bool set)
{
    if(set) currentShader_ = 1;
    update();
}

void GLWidget::SetFresnelR(double r) {
    fresnel_[0] = r;
    update();
}

void GLWidget::SetFresnelG(double g) {
    fresnel_[1] = g;
    update();
}

void GLWidget::SetCurrentTexture(int i)
{
    currentTexture_ = i;
    update();
}

void GLWidget::SetSkyVisible(bool set)
{
    skyVisible_ = set;
    update();
}

void GLWidget::SetFresnelB(double b) {
    fresnel_[2] = b;
    update();
}

void GLWidget::SetMetalness(double d) {
    metalness_ = d;
    update();
}

void GLWidget::SetRoughness(double d) {
    roughness_ = d;
    update();
}

void GLWidget::SetTwoStep(bool set) {
  twoStep = set;
  if(set) currentTwoStepShader_ = 0;
  update();
}

void GLWidget::SetTwoStepMode(int i) {
  twoStepMode = i;
  update();
}

void GLWidget::SetAO(bool set) {
  twoStep = set;
  if (set) currentTwoStepShader_ = 1;
  update();
}

void GLWidget::SetRadius(double r) {
  radius = r;
  update();
}

void GLWidget::SetSamples(int sampl) {
  nSamples = sampl;
  update();
}

void GLWidget::SetDirections(int dir) {
  nDirections = dir; 
  update();
}

void GLWidget::SetAOAlbedo(bool set) {
  AOAlbedo = set;
  update();
}

void GLWidget::SetPBR(bool set) {
  twoStep = !set;
  update();
}

void GLWidget::SetImprovedSSAO(bool set) {
  twoStep = set;
  if (set) currentTwoStepShader_ = 2;
  imrpovedAO = set;
}