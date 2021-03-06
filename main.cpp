#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#include <GL/gl.h>
#include "imageloader.h"
#include "vec3f.h"
#endif

static GLfloat spin,muter = 0.0;
static int posx=0,posy=2.0,posz =0,tree=0,z=0, home=0;
float angle = 0;
using namespace std;


float lastx, lasty;
GLint stencilBits;
static int viewx = 50;
static int viewy = 24;
static int viewz = 80;

float rot = 0;

struct ImageTexture {
	unsigned long sizeX;
	unsigned long sizeY;
	char *data;
};
typedef struct ImageTexture ImageTexture; //struktur data untuk

const GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
const GLfloat light_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 1.0f };

const GLfloat light_ambient2[] = { 0.3f, 0.3f, 0.3f, 0.0f };
const GLfloat light_diffuse2[] = { 0.3f, 0.3f, 0.3f, 0.0f };

const GLfloat mat_ambient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat high_shininess[] = { 100.0f };

unsigned int LoadTextureFromBmpFile(char *filename);


//train 2D
//class untuk terain 2D
class Terrain {
private:
	int w; //Width
	int l; //Length
	float** hs; //Heights
	Vec3f** normals;
	bool computedNormals; //Whether normals is up-to-date
public:
	Terrain(int w2, int l2) {
		w = w2;
		l = l2;

		hs = new float*[l];
		for (int i = 0; i < l; i++) {
			hs[i] = new float[w];
		}

		normals = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals[i] = new Vec3f[w];
		}

		computedNormals = false;
	}

	~Terrain() {
		for (int i = 0; i < l; i++) {
			delete[] hs[i];
		}
		delete[] hs;

		for (int i = 0; i < l; i++) {
			delete[] normals[i];
		}
		delete[] normals;
	}

	int width() {
		return w;
	}

	int length() {
		return l;
	}

	//Sets the height at (x, z) to y
	void setHeight(int x, int z, float y) {
		hs[z][x] = y;
		computedNormals = false;
	}

	//Returns the height at (x, z)
	float getHeight(int x, int z) {
		return hs[z][x];
	}

	//Computes the normals, if they haven't been computed yet
	void computeNormals() {
		if (computedNormals) {
			return;
		}

		//Compute the rough version of the normals
		Vec3f** normals2 = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals2[i] = new Vec3f[w];
		}

		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum(0.0f, 0.0f, 0.0f);

				Vec3f out;
				if (z > 0) {
					out = Vec3f(0.0f, hs[z - 1][x] - hs[z][x], -1.0f);
				}
				Vec3f in;
				if (z < l - 1) {
					in = Vec3f(0.0f, hs[z + 1][x] - hs[z][x], 1.0f);
				}
				Vec3f left;
				if (x > 0) {
					left = Vec3f(-1.0f, hs[z][x - 1] - hs[z][x], 0.0f);
				}
				Vec3f right;
				if (x < w - 1) {
					right = Vec3f(1.0f, hs[z][x + 1] - hs[z][x], 0.0f);
				}

				if (x > 0 && z > 0) {
					sum += out.cross(left).normalize();
				}
				if (x > 0 && z < l - 1) {
					sum += left.cross(in).normalize();
				}
				if (x < w - 1 && z < l - 1) {
					sum += in.cross(right).normalize();
				}
				if (x < w - 1 && z > 0) {
					sum += right.cross(out).normalize();
				}

				normals2[z][x] = sum;
			}
		}

		//Smooth out the normals
		const float FALLOUT_RATIO = 0.5f;
		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum = normals2[z][x];

				if (x > 0) {
					sum += normals2[z][x - 1] * FALLOUT_RATIO;
				}
				if (x < w - 1) {
					sum += normals2[z][x + 1] * FALLOUT_RATIO;
				}
				if (z > 0) {
					sum += normals2[z - 1][x] * FALLOUT_RATIO;
				}
				if (z < l - 1) {
					sum += normals2[z + 1][x] * FALLOUT_RATIO;
				}

				if (sum.magnitude() == 0) {
					sum = Vec3f(0.0f, 1.0f, 0.0f);
				}
				normals[z][x] = sum;
			}
		}

		for (int i = 0; i < l; i++) {
			delete[] normals2[i];
		}
		delete[] normals2;

		computedNormals = true;
	}

	//Returns the normal at (x, z)
	Vec3f getNormal(int x, int z) {
		if (!computedNormals) {
			computeNormals();
		}
		return normals[z][x];
	}
};
//end class


//Loads a terrain from a heightmap.  The heights of the terrain range from
//-height / 2 to height / 2.
//load terain di procedure inisialisasi
Terrain* loadTerrain(const char* filename, float height) {
	Image* image = loadBMP(filename);
	Terrain* t = new Terrain(image->width, image->height);
	for (int y = 0; y < image->height; y++) {
		for (int x = 0; x < image->width; x++) {
			unsigned char color = (unsigned char) image->pixels[3 * (y
					* image->width + x)];
			float h = height * ((color / 255.0f) - 0.5f);
			t->setHeight(x, y, h);
		}
	}

	delete image;
	t->computeNormals();
	return t;
}


float _angle = 60.0f;
//buat tipe data terain
Terrain* _terrain;
Terrain* _terrainTanah;
Terrain* _terrainAir;


void cleanup() {
	delete _terrain;
	delete _terrainTanah;
}


//mengambil gambar BMP
int ImageLoad(char *filename, ImageTexture *imageTex) {
	FILE *file;
	unsigned long size; // ukuran image dalam bytes
	unsigned long i; // standard counter.
	unsigned short int plane; // number of planes in image

	unsigned short int bpp; // jumlah bits per pixel
	char temp; // temporary color storage for var warna sementara untuk memastikan filenya ada


	if ((file = fopen(filename, "rb")) == NULL) {
		printf("File Not Found : %s\n", filename);
		return 0;
	}
	// mencari file header bmp
	fseek(file, 18, SEEK_CUR);
	// read the width
	if ((i = fread(&imageTex->sizeX, 4, 1, file)) != 1) {
		printf("Error reading width from %s.\n", filename);
		return 0;
	}

	if ((i = fread(&imageTex->sizeY, 4, 1, file)) != 1) {
		printf("Error reading height from %s.\n", filename);
		return 0;
	}

	size = imageTex->sizeX * imageTex->sizeY * 3;
	// read the planes
	if ((fread(&plane, 2, 1, file)) != 1) {
		printf("Error reading planes from %s.\n", filename);
		return 0;
	}
	if (plane != 1) {
		printf("Planes from %s is not 1: %u\n", filename, plane);
		return 0;
	}
	// read the bitsperpixel
	if ((i = fread(&bpp, 2, 1, file)) != 1) {
		printf("Error reading bpp from %s.\n", filename);

		return 0;
	}
	if (bpp != 24) {
		printf("Bpp from %s is not 24: %u\n", filename, bpp);
		return 0;
	}
	// seek past the rest of the bitmap header.
	fseek(file, 24, SEEK_CUR);
	// read the data.
	imageTex->data = (char *) malloc(size);
	if (imageTex->data == NULL) {
		printf("Error allocating memory for color-corrected image data");
		return 0;
	}
	if ((i = fread(imageTex->data, size, 1, file)) != 1) {
		printf("Error reading image data from %s.\n", filename);
		return 0;
	}
	for (i = 0; i < size; i += 3) { // membalikan semuan nilai warna (gbr - > rgb)
		temp = imageTex->data[i];
		imageTex->data[i] = imageTex->data[i + 2];
		imageTex->data[i + 2] = temp;
	}
	// we're done.
	return 1;
}

//mengambil tekstur
ImageTexture * loadTexture() {
	ImageTexture *image1;
	// alokasi memmory untuk tekstur
	image1 = (ImageTexture *) malloc(sizeof(ImageTexture));
	if (image1 == NULL) {
		printf("Error allocating space for image");
		exit(0);
	}
	//pic.bmp is a 64x64 picture
	if (!ImageLoad("water.bmp", image1)) {
		exit(1);
	}
	return image1;
}

void initRendering() {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);
}

void drawScene() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	float scale = 500.0f / max(_terrain->width() - 1, _terrain->length() - 1);
	glScalef(scale, scale, scale);
	glTranslatef(-(float) (_terrain->width() - 1) / 2, 0.0f,
			-(float) (_terrain->length() - 1) / 2);

	glColor3f(0.3f, 0.9f, 0.0f);
	//glColor3f(0.5f, 0.9f, 0.0f);
	for (int z = 0; z < _terrain->length() - 1; z++) {
		//Makes OpenGL draw a triangle at every three consecutive vertices
		glBegin(GL_TRIANGLE_STRIP);
		for (int x = 0; x < _terrain->width(); x++) {
			Vec3f normal = _terrain->getNormal(x, z);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, _terrain->getHeight(x, z), z);
			normal = _terrain->getNormal(x, z + 1);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, _terrain->getHeight(x, z + 1), z + 1);
		}
		glEnd();
	}

}
//untuk di display
void drawSceneTanah(Terrain *terrain, GLfloat r, GLfloat g, GLfloat b) {

	float scale = 500.0f / max(terrain->width() - 1, terrain->length() - 1);
	glScalef(scale, scale, scale);
	glTranslatef(-(float) (terrain->width() - 1) / 2, 0.0f,
			-(float) (terrain->length() - 1) / 2);

	glColor3f(r, g, b);
	for (int z = 0; z < terrain->length() - 1; z++) {
		//Makes OpenGL draw a triangle at every three consecutive vertices
		glBegin(GL_TRIANGLE_STRIP);
		for (int x = 0; x < terrain->width(); x++) {
			Vec3f normal = terrain->getNormal(x, z);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z), z);
			normal = terrain->getNormal(x, z + 1);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z + 1), z + 1);
		}
		glEnd();
	}

}

void update(int value) {
	 _angle += 1.0f;
	 if (_angle > 360) {
	 _angle -= 360;
	 }
	glutPostRedisplay();
	glutTimerFunc(25, update, 0);
}

GLuint texture[40];
void freetexture(GLuint texture) {
	glDeleteTextures(1, &texture);
}

GLuint loadtextures(const char *filename, int width, int height) {
	GLuint texture;

	unsigned char *data;
	FILE *file;

	file = fopen(filename, "rb");
	if (file == NULL)
		return 0;

	data = (unsigned char *) malloc(width * height * 3);
	fread(data, width * height * 3, 1, file);

	fclose(file);

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			GL_LINEAR_MIPMAP_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(  GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	gluBuild2DMipmaps(GL_TEXTURE_2D, 3, width, height, GL_RGB,
			GL_UNSIGNED_BYTE, data);

	data = NULL;

	return texture;
}

void rumah()
{
    glPushMatrix();
    glTranslatef(-5.0f,0.0f,-3.0f);

    //atap kanan
    glBegin(GL_POLYGON);
    glColor3f(1.0f, 0.4f, 0.0f);
        glTexCoord2f(1.0,0.0);glVertex3f(0.75,0.7,0.0);
        glTexCoord2f(1.0,1.0);glVertex3f(1.5,0.5,0.0);
        glTexCoord2f(0.0,1.0);glVertex3f(1.5,0.5,-1.8);
        glTexCoord2f(0.0,0.0);glVertex3f(0.75,0.7,-1.8);
    glEnd();

    //atap kiri
    glBegin(GL_POLYGON);
        glTexCoord2f(1.0,0.0);glVertex3f(0.75,0.7,0.0);
        glTexCoord2f(1.0,1.0);glVertex3f(0.0,0.5,0.0);
        glTexCoord2f(0.0,1.0);glVertex3f(0.0,0.5,-1.8);
        glTexCoord2f(0.0,0.0);glVertex3f(0.75,0.7,-1.8);
    glEnd();

    //bagian atas
    glBegin(GL_POLYGON);
    glColor3f(1.0f,0.4f,0.0f);
        glVertex3f(0.0,0.5,0.0);
        glVertex3f(1.5,0.5,0.0);
        glVertex3f(1.5,0.5,-1.8);
        glVertex3f(0.0,0.5,-1.8);
    glEnd();
    glBindTexture(GL_TEXTURE_2D,texture[11]);
    //bagian belakang
    glBegin(GL_POLYGON);
    //glColor3f(0.1f,0.0f,0.0f);
        glTexCoord2f(0.0,0.0);glVertex3f(0.0,0.0,-1.8);
        glTexCoord2f(1.0,0.0);glVertex3f(1.5,0.0,-1.8);
        glTexCoord2f(1.0,1.0);glVertex3f(1.5,0.5,-1.8);
        glTexCoord2f(0.0,1.0);glVertex3f(0.0,0.5,-1.8);
    glEnd();

    //bagian kanan
    glBegin(GL_POLYGON);
        glTexCoord2f(1.0,0.0);glVertex3f(0.0,0.0,0.0);
        glTexCoord2f(1.0,1.0);glVertex3f(0.0,0.5,0.0);
        glTexCoord2f(0.0,1.0);glVertex3f(0.0,0.5,-1.8);
        glTexCoord2f(0.0,0.0);glVertex3f(0.0,0.0,-1.8);
    glEnd();

    //bagian tutup kanan
    glBegin(GL_POLYGON);
        glTexCoord2f(0.0,0.0);glVertex3f(0.0,0.0,0.0);
        glTexCoord2f(1.0,0.0);glVertex3f(0.5,0.0,0.0);
        glTexCoord2f(1.0,1.0);glVertex3f(0.5,0.5,0.0);
        glTexCoord2f(0.0,1.0);glVertex3f(0.0,0.5,0.0);
    glEnd();

    //bagian kiri
    glBegin(GL_POLYGON);
        glTexCoord2f(1.0,0.0);glVertex3f(1.5,0.0,0.0);
        glTexCoord2f(1.0,1.0);glVertex3f(1.5,0.5,0.0);
        glTexCoord2f(0.0,1.0);glVertex3f(1.5,0.5,-1.8);
        glTexCoord2f(0.0,0.0);glVertex3f(1.5,0.0,-1.8);
    glEnd();

    //bagian tutup kiri
    glBegin(GL_POLYGON);
        glTexCoord2f(0.0,0.0);glVertex3f(1.5,0.0,0.0);
        glTexCoord2f(1.0,0.0);glVertex3f(1,0.0,0.0);
        glTexCoord2f(1.0,1.0);glVertex3f(1,0.5,0.0);
        glTexCoord2f(0.0,1.0);glVertex3f(1.5,0.5,0.0);
    glEnd();

    glBindTexture(GL_TEXTURE_2D,texture[10]);
    //pintu
    glBegin(GL_POLYGON);
        glTexCoord2f(0.0,0.0);glVertex3f(0.5,0.0,0.0);
        glTexCoord2f(1.0,0.0);glVertex3f(1,0.0,0.0);
        glTexCoord2f(1.0,1.0);glVertex3f(1,0.5,0.0);
        glTexCoord2f(0.0,1.0);glVertex3f(0.5,0.5,0.0);
    glEnd();

    glPopMatrix();
}



void pohon(void){
    //batang
    GLUquadricObj *pObj;
    pObj =gluNewQuadric();
    gluQuadricNormals(pObj, GLU_SMOOTH);

    glPushMatrix();
    glColor3ub(104,70,14);
    glRotatef(270,1,0,0);
    gluCylinder(pObj, 4, 0.7, 30, 25, 25);
    glPopMatrix();
}

//ranting
void ranting(void){
    GLUquadricObj *pObj;
    pObj =gluNewQuadric();
    gluQuadricNormals(pObj, GLU_SMOOTH);
    glPushMatrix();
    glColor3ub(104,70,14);
    glTranslatef(0,27,0);
    glRotatef(330,1,0,0);
    gluCylinder(pObj, 0.6, 0.1, 15, 25, 25);
    glPopMatrix();

    //daun
    glPushMatrix();
    glColor3ub(18,118,13);
    glScaled(5, 5, 5);
    glTranslatef(0,7,3);
    glutSolidDodecahedron();
    glPopMatrix();
}



//Tanah
void tanah(void) {
	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[2]);
	//glColor4f(1, 1, 1, 1);
	glRotatef(180, 0, 0, 1);
	glScalef(80, 0, 120);
	glBegin(GL_QUADS);
	glTexCoord2f(1, 0);
	glVertex3f(-1, -1, 1);
	glTexCoord2f(1, 1);
	glVertex3f(-1, 1, -0.0);
	glTexCoord2f(0, 1);
	glVertex3f(1, 1, -0.0);
	glTexCoord2f(0, 0);
	glVertex3f(1, -1, 1);
	glEnd();
	glPopMatrix();
}

void awan(void){
    glPushMatrix();
    glColor3ub(153, 223, 255);
    glutSolidSphere(10, 50, 50);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(10,0,1);
    glutSolidSphere(5, 50, 50);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-2,6,-2);
    glutSolidSphere(7, 50, 50);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-10,-3,0);
    glutSolidSphere(7, 50, 50);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(6,-2,2);
    glutSolidSphere(7, 50, 50);
    glPopMatrix();
}



void display(void) {
	glClearStencil(0); //clear the stencil buffer
	glClearDepth(1.0f);
	glClearColor(0.0, 0.6, 0.8, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); //clear the buffers
	glLoadIdentity(); // reset posisi
	//gluLookAt(viewx, viewy, viewz, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
    gluLookAt(viewx+50, viewy+200, viewz+200, 0.0, 10.0, 5.0, 0.0, 1.0, 0.0);

	glPushMatrix();
	drawScene();
	glPopMatrix();

	glPushMatrix();
    drawSceneTanah(_terrainAir, 0.0f, 0.1f, 0.5f);
	glPopMatrix();

    for (home=0;home<=7;home=home+3)
    {
        //rumah menghadap jembatan
        glPushMatrix();
        glBindTexture(GL_TEXTURE_2D,texture[12]);
        glPushMatrix();
        glScaled(20,50,20);
        glTranslatef(10-home,0.2,-1);
        glColor3f(0.2,0.4,0.5);
        rumah();
        glPopMatrix();
    }

    for (home=0;home<=7;home=home+3)
    {
        // rumah menghadap kanan
        glPushMatrix();
        glBindTexture(GL_TEXTURE_2D,texture[12]);
        glScaled(20, 50, 20);
        glTranslatef(0, 0.2, -8+home);
        glRotatef(90,0,1,0);
        glColor3f(0.2,0.4,0.5);
        rumah();
        glPopMatrix();
    }



    for (home=0;home<=17;home=home+7)
    {
        // rumah menghadap kanan belakang
        glPushMatrix();
        glBindTexture(GL_TEXTURE_2D,texture[12]);
        glScaled(20, 50, 20);
        glTranslatef(-7.5, 0.2, -11.3+home);
        glRotatef(90,0,1,0);
        glColor3f(0.2,0.4,0.5);
        rumah();
        glPopMatrix();
    }
    for (home=0;home<=5;home=home+3)
    {
        glPushMatrix();
        glBindTexture(GL_TEXTURE_2D,texture[12]);
        glScaled(20, 50, 20);
        glTranslatef(6.5, 0.2, 9-home);
        glRotatef(260,0,1,0);
        glColor3f(0.2,0.4,0.5);
        rumah();
        glPopMatrix();
    }
    glPopMatrix();

    //pohon pojok kanan bawah
    glPushMatrix();
    glTranslatef(150,-13,150);
    glScalef(1.2, 2, 1.0);
    glRotatef(70,0,1,0);
    pohon();


    //ranting1
    ranting(); // ranting kanan

    //ranting2
    glPushMatrix();
    glScalef(1.5, 1.5, 1.5);
    glTranslatef(0,25,25);
    glRotatef(250,1,0,0);
    ranting();
    glPopMatrix();

    //ranting3
    glPushMatrix();
    glScalef(1.8, 1.8, 1.8);
    glTranslatef(0,-6,21.5);
    glRotatef(-55,1,0,0);
    ranting();
    glPopMatrix();

    glPopMatrix();

    //pohon tengah
    glPushMatrix();
    glTranslatef(150,-13,0);
    glScalef(1.2, 2, 1.0);
    glRotatef(90,0,1,0);
    pohon();


    //ranting1
    ranting(); // ranting kanan

    //ranting2
    glPushMatrix();
    glScalef(1.5, 1.5, 1.5);
    glTranslatef(0,25,25);
    glRotatef(250,1,0,0);
    ranting();
    glPopMatrix();

    //ranting3
    glPushMatrix();
    glScalef(1.8, 1.8, 1.8);
    glTranslatef(0,-6,21.5);
    glRotatef(-55,1,0,0);
    ranting();
    glPopMatrix();

    glPopMatrix();


    //pohon bawah
    glPushMatrix();
    glTranslatef(-80,-13,150);
    glScalef(1.2, 2, 1.0);
    glRotatef(360,0,1,0);
    pohon();


    //ranting1
    ranting(); // ranting kanan

    //ranting2
    glPushMatrix();
    glScalef(1.5, 1.5, 1.5);
    glTranslatef(0,25,25);
    glRotatef(250,1,0,0);
    ranting();
    glPopMatrix();

    //ranting3
    glPushMatrix();
    glScalef(1.8, 1.8, 1.8);
    glTranslatef(0,-6,21.5);
    glRotatef(-55,1,0,0);
    ranting();
    glPopMatrix();

    glPopMatrix();

    glPopMatrix();

    //pohon kiri pinggir
    glPushMatrix();
    glTranslatef(-200,-13,50);
    glScalef(1.2, 2, 1.0);
    glRotatef(120,0,1,0);
    pohon();


    //ranting1
    ranting(); // ranting kanan

    //ranting2
    glPushMatrix();
    glScalef(1.5, 1.5, 1.5);
    glTranslatef(0,25,25);
    glRotatef(250,1,0,0);
    ranting();
    glPopMatrix();

    //ranting3
    glPushMatrix();
    glScalef(1.8, 1.8, 1.8);
    glTranslatef(0,-6,21.5);
    glRotatef(-55,1,0,0);
    ranting();
    glPopMatrix();

    glPopMatrix();

    //awan
    glPushMatrix();
    glTranslatef(-75, 110, -120);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-45, 110, -115);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-50, 120, -120);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-140, 90, -120);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-155, 90, -115);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-130, 110, -120);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-190, 110, -120);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-175, 120, -115);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-200, 100, -120);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-30, 110, -120);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-35, 95, -115);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-20, 90, -120);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-80, 90, -120);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(220, 90, -120);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(180, 90, -120);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();


    glPushMatrix();
    glTranslatef(190, 110, -120);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(125, 110, -115);
    glScalef(1.8, 1.0, 1.0);
    awan();
    glPopMatrix();


	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); //disable the color mask
	glDepthMask(GL_FALSE); //disable the depth mask

	glEnable(GL_STENCIL_TEST); //enable the stencil testing

	glStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF);
	glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE); //set the stencil buffer to replace our next lot of data

	//ground
	tanah(); //set the data plane to be replaced
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); //enable the color mask
	glDepthMask(GL_TRUE); //enable the depth mask

	glStencilFunc(GL_EQUAL, 1, 0xFFFFFFFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP); //set the stencil buffer to keep our next lot of data

	glDisable(GL_DEPTH_TEST); //disable depth testing of the reflection

	// glPopMatrix();
	glEnable(GL_DEPTH_TEST); //enable the depth testing
	glDisable(GL_STENCIL_TEST); //disable the stencil testing
	//end of ground
	glEnable(GL_BLEND); //enable alpha blending
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //set the blending function
	glRotated(1, 0, 0, 0);


	glutSwapBuffers();
	glFlush();
	rot++;
	angle++;
}

void init(void) {
    initRendering();
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glDepthFunc(GL_LESS);
	glEnable(GL_NORMALIZE);
	glEnable(GL_COLOR_MATERIAL);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	//glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	_terrain = loadTerrain("heightmap.bmp", 20);
	_terrainTanah = loadTerrain("heightmapTanah.bmp", 20);
	_terrainAir = loadTerrain("heightmapAir.bmp", 20);
	texture[2] = loadtextures("rumput1.bmp",600,450);
	texture[10] = loadtextures("pintu.bmp",340,683);
	texture[11] = loadtextures("dinding.bmp",128,129);
	texture[12] = loadtextures("atap.bmp",600,597);

	//binding texture

	ImageTexture *image2 = loadTexture();

	if (image2 == NULL) {
		printf("Image was not returned from loadTexture\n");
		exit(0);
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Generate texture/ membuat texture
	glGenTextures(3, texture);

	//binding texture untuk membuat texture 2D
	glBindTexture(GL_TEXTURE_2D, texture[0]);

	//menyesuaikan ukuran textur ketika image lebih besar dari texture
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); //
	//menyesuaikan ukuran textur ketika image lebih kecil dari texture
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //

	glTexImage2D(GL_TEXTURE_2D, 0, 3, image2->sizeX, image2->sizeY, 0, GL_RGB,
			GL_UNSIGNED_BYTE, image2->data);

}

static void keyB(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_HOME:
		viewy++;
		break;
	case GLUT_KEY_END:
		viewy--;
		break;
	case GLUT_KEY_UP:
		viewz--;
		break;
	case GLUT_KEY_DOWN:
		viewz++;
		break;

	case GLUT_KEY_RIGHT:
		viewx++;
		break;
	case GLUT_KEY_LEFT:
		viewx--;
		break;

	case GLUT_KEY_F1: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	case GLUT_KEY_F2: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient2);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse2);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	default:
		break;
	}
}

void keyboard(unsigned char key, int x, int y) {
    if (key == 'm') {

		muter = muter - 1;
		if (muter > 360.0)
			muter = muter - 360.0;
	}
	if (key == 'n') {
		muter = muter + 1;
		if (muter > 360.0)
			muter = muter - 360.0;
	}
	if (key == 'q') {
		viewz++;
	}
	if (key == 'e') {
		viewz--;
	}
	if (key == 's') {
		viewy--;
	}
	if (key == 'w') {
		viewy++;
	}

}

void reshape(int w, int h) {
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, (GLfloat) w / (GLfloat) h, 0.1, 1000.0);
	glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char **argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_STENCIL | GLUT_DEPTH); //add a stencil buffer to the window
	glutInitWindowSize(800, 600);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("Pedesaan");
	init();

	glutDisplayFunc(display);
	glutIdleFunc(display);
	glutReshapeFunc(reshape);
	glutSpecialFunc(keyB);

	glutKeyboardFunc(keyboard);

	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);

	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess);
	glColorMaterial(GL_FRONT, GL_DIFFUSE);

	glutMainLoop();
	return 0;
}
