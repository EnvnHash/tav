/*
 * Cylinder.cpp
 *
 *  Created on: Jul 12, 2018
 *      Author: sven
 */

#include "GeoPrimitives/Cylinder.h"

namespace tav
{

Cylinder::Cylinder(unsigned int _nrSegs, std::vector<coordType>* _instAttribs,
		int _maxNrInstances) : GeoPrimitive(), nrSegs(_nrSegs), instAttribs(_instAttribs),
				maxNrInstances(_maxNrInstances)
{
	init();
}

//--------------------------------------------------------------------------------

void Cylinder::init()
{
	float x, z, fInd;
	float cylRadius = 1.f;
	float capRadius = 0.13f;

	// the cylinder will be normalized from (-1|-1|-1) to (1|1|1)
	// triangles with indices define Triangles
	// we will define the cylinder body and the caps separately so that we can use smooth normals
	// (and have a hard edge on the top -> so different normals at same positions)

	// define a ring in the x, z plain
	GLfloat* ringPos = new GLfloat[nrSegs *2];

	for (unsigned int i=0; i<nrSegs; i++) {

		// define a circle with n points
		fInd = float (i) / float(nrSegs);
		x = std::cos(fInd * float(M_PI) * 2.f);
		z = std::sin(fInd * float(M_PI) * 2.f);

		ringPos[i*2] = x;
		ringPos[i*2 +1] = z;

		//printf("ringPos [%d] %f %f \n", i, ringPos[i*2], ringPos[i*2 +1]);
	}

	// ------- cylinder body ---------

	// allocate memory for all positions and normals
	// two rings with each three coordinates (x, y, z)
	// two caps with each three coordinates (x, y, z)
	// one center point on each cap

	unsigned int totNrVert = nrSegs *4 +2;
	GLfloat* positions = new GLfloat[totNrVert *3];
	GLfloat* normals = new GLfloat[totNrVert *3];

	// define the two rings
	// 0: the cylinder bottom
	// 1: the cylinder top
	unsigned int ind=0;
	for (unsigned int ringNr=0; ringNr<2; ringNr++)
	{
		for (unsigned int i=0; i<nrSegs; i++)
		{
			positions[ind*3] = ringPos[i*2] * cylRadius;
			positions[ind*3 +1] = ringNr == 0 ? -1.f : 1.f;
			positions[ind*3 +2] = ringPos[i*2 +1] * cylRadius;

			normals[ind*3] = ringPos[i*2];
			normals[ind*3 +1] = 0.f;
			normals[ind*3 +2] = ringPos[i*2 +1];

			ind++;
		}
	}

	// cap centers
	for (unsigned int i=0; i<2; i++)
	{
		positions[ind*3]	= 0.f;
		positions[ind*3 +1] = i == 0 ? -1.f : 1.f;
		positions[ind*3 +2] = 0.f;

		normals[ind*3]	= 0.f;
		normals[ind*3 +1] = i == 0 ? -1.f : 1.f;
		normals[ind*3 +2] = 0.f;

		ind++;
	}

	// cap rings
	for (unsigned int ringNr=0; ringNr<2; ringNr++)
	{
		for (unsigned int i=0; i<nrSegs; i++)
		{
			positions[ind*3] = ringPos[i*2] * cylRadius;
			positions[ind*3 +1] = ringNr == 0 ? -1.f : 1.f;
			positions[ind*3 +2] = ringPos[i*2 +1] * cylRadius;

			normals[ind*3]	= 0.f;
			normals[ind*3 +1] = ringNr == 0 ? -1.f : 1.f;
			normals[ind*3 +2] = 0.f;

			ind++;
		}
	}


	// create Indices
	// for the cylinder we need one quad of two triangles per ringPoint = nrSegs *6 Vertices
	// for each cap we need = nrSegs * 3 Vertices, since we have two caps nrSegs *6
	// so in total we need nrSegs *12
	GLuint* cyl_indices = new GLuint[nrSegs *12];

	//  clockwise (viewed from the camera)
	GLuint oneQuadTemp[6] = { 0, 0, 1, 1, 0, 1 };
	GLuint upDownTemp[6] = { 0, 1, 0, 0, 1, 1 };  // 0 = bottom, 1 ==top

	ind = 0;

	// cylinder body
	for (unsigned int i=0; i<nrSegs; i++)
		for (unsigned int j=0; j<6; j++)
			cyl_indices[ind++] = ((oneQuadTemp[j] +i) % nrSegs) + (nrSegs * upDownTemp[j]);

	// cap bottom and cap top
	unsigned int capCenterInd = nrSegs *2;
	unsigned int posIndOffs;

	// cap bottom and top = 2
	for (unsigned int k=0; k<2; k++){

		posIndOffs = nrSegs *2 +2 + nrSegs*k;

		for (unsigned int i=0; i<nrSegs; i++){

			for (unsigned int j=0; j<3; j++) {

				switch (j){
				case 0:
					cyl_indices[ind++] = capCenterInd +k;
					break; // always center of cap / tip
				case 1:
					cyl_indices[ind++] = ((k == 1 ? i+1 : i) % nrSegs) + posIndOffs;
					break;
				case 2:
					cyl_indices[ind++] = ((k == 1 ? i : i+1) % nrSegs) + posIndOffs;
					break;
				default: break;
				}
			}
		}
	}

	GLenum usage = GL_STATIC_DRAW;
	if (instAttribs)
		usage = GL_DYNAMIC_DRAW;

	format = "position:3f,normal:3f";
	vao = new VAO(format, usage, instAttribs, maxNrInstances);
	vao->upload(POSITION, &positions[0], totNrVert);
	vao->upload(NORMAL, &normals[0], totNrVert);
	vao->setElemIndices(nrSegs * 12, &cyl_indices[0]);

	totNrPoints = totNrVert;

	delete [] ringPos;
	delete [] positions;
	delete [] normals;
	delete [] cyl_indices;
}

//--------------------------------------------------------------------------------

void Cylinder::remove(){

}

//--------------------------------------------------------------------------------
/*
void Cylinder::draw(double time, double dt, camPar* cp, Shaders* _shader, renderPass _pass, TFO* _tfo){

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	// clear the depth buffer, gizmo should be always visible
	if ((_pass == MC3D_SCENE_PASS || _pass == MC3D_OBJECT_MAP_PASS) && _nameFlag & MC3D_TRANS_GIZMO_Z )
		glClear(GL_DEPTH_BUFFER_BIT);

	if (_pass == MC3D_SCENE_PASS || _pass == MC3D_OBJECT_MAP_PASS) {

		// material
		_shader->setUniform1i("hasTexture", 0);
		_shader->setUniform1i("shadingMode", (int)MC3D_SHADINGMODE_CONTR_ELEM);
		_shader->setUniform4f("ambient", 0.f, 0.f, 0.f, 0.f);
		_shader->setUniform4f("emissive", emissScale * gColor.r, emissScale * gColor.g, emissScale * gColor.b, emissScale * gColor.a);
		_shader->setUniform4fv("diffuse",  glm::value_ptr(gColor));
		_shader->setUniform4f("specular", 1.f, 1.f, 1.f, 1.f);

		gizVao->drawElements(GL_TRIANGLES, NULL, GL_TRIANGLES, totNrPoints);
	}
}
*/
//--------------------------------------------------------------------------------

Cylinder::~Cylinder() {}

}
