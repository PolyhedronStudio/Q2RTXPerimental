#include <iostream>
#include <vector>

#define M_PI_2 		1.57079632679489661923 // PI/2
#include "../../inc/shared/shared.h"
#define PITCH0 // up / down
#define YAW1 // left / right
#define ROLL2 // fall over

#ifndef CLGAME_HARD_LINKED
/**
*	@brief
**/
void Com_LPrintf( print_type_t type, const char *fmt, ... ) {
	va_list     argptr;
	char        text[ MAX_STRING_CHARS ];

	if ( type == PRINT_DEVELOPER ) {
		return;
	}

	va_start( argptr, fmt );
	vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );

	//clgi.Print( type, "%s", text );
	std::cout << "[LOG]: " << text;
}
/**
*	@brief
**/
void Com_Error( error_type_t type, const char *fmt, ... ) {
	va_list     argptr;
	char        text[ MAX_STRING_CHARS ];

	va_start( argptr, fmt );
	vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );

	std::cout << "[ERROR]: " << text;
}
#endif

using namespace std;
static constexpr float EPS =1e-5f;

static bool feq(float a, float b, float eps = EPS) { return  fabsf(a - b) <= eps; }

Vector3 transformPoint(const Matrix &m, const Vector3 &v) {
 // Uses the same convention as the functions in the header:
 // x' = m0*x + m4*y + m8*z + m12
 // y' = m1*x + m5*y + m9*z + m13
 // z' = m2*x + m6*y + m10*z + m14
 return  Vector3{ m.m0 * v.x + m.m4 * v.y + m.m8 * v.z + m.m12,
 m.m1 * v.x + m.m5 * v.y + m.m9 * v.z + m.m13,
 m.m2 * v.x + m.m6 * v.y + m.m10 * v.z + m.m14 };
}

Vector3 transformVector(const Matrix &m, const Vector3 &v) {
 // same but ignore translation
 return  Vector3{ m.m0 * v.x + m.m4 * v.y + m.m8 * v.z,
 m.m1 * v.x + m.m5 * v.y + m.m9 * v.z,
 m.m2 * v.x + m.m6 * v.y + m.m10 * v.z };
}

bool checkVec(const Vector3 &a, const Vector3 &b, float eps = EPS) {
 return  feq(a.x, b.x, eps) && feq(a.y, b.y, eps) && feq(a.z, b.z, eps);
}

void printVec(const char *name, const Vector3 &v) {
 std::cout << name << ": [" << v.x << ", " << v.y << ", " << v.z << "]\n";
}

static bool compareMatrices(const Matrix &a, const Matrix &b, float eps =1e-5f) {
 const float *pa = &a.m0;
 const float *pb = &b.m0;
 for (int i =0; i <16; ++i) {
 if (!feq(pa[i], pb[i], eps)) return  false;
 }
 return  true;
}

int test_rotations() {
 std::cout << "Test rotations...\n";
 // X90 degrees
 Matrix rx = QM_MatrixRotateX( M_PI_2 ); // pi/2
 Vector3 v = {0.0f,1.0f,0.0f };
 Vector3 out = transformVector(rx, v);
 Vector3 expectX = {0.0f,0.0f,1.0f }; // (0,1,0) -> (0,0,1)
 if (!checkVec(out, expectX)) {
 printVec("rx * (0,1,0)", out); printVec("expected", expectX);
 return  1;
 }

 // Y90 degrees: (0,0,1) -> (1,0,0)
 Matrix ry = QM_MatrixRotateY( M_PI_2 );
 v = {0.0f,0.0f,1.0f };
 out = transformVector(ry, v);
 Vector3 expectY = {1.0f,0.0f,0.0f };
 if (!checkVec(out, expectY)) {
 printVec("ry * (0,0,1)", out); printVec("expected", expectY);
 return  1;
 }

 // Z90 degrees: (1,0,0) -> (0,1,0) given your Z+ up convention and implemented matrix
 Matrix rz = QM_MatrixRotateZ( M_PI_2 );
 v = {1.0f,0.0f,0.0f };
 out = transformVector(rz, v);
 Vector3 expectZ = {0.0f,1.0f,0.0f };
 if (!checkVec(out, expectZ)) {
 printVec("rz * (1,0,0)", out); printVec("expected", expectZ);
 return  1;
 }

 std::cout << "Rotation tests passed.\n";
 return 0;
}

int test_orthonormality_and_inverse_transpose() {
 std::cout << "Test orthonormality and inverse == transpose for rotation matrices...\n";
 Matrix r = QM_MatrixRotateXYZ( Vector3{0.4f,0.7f, -0.2f } );
 // Extract3x3 columns/rows as used by transformVector
 Vector3 colX = { r.m0, r.m1, r.m2 };
 Vector3 colY = { r.m4, r.m5, r.m6 };
 Vector3 colZ = { r.m8, r.m9, r.m10 };

 // dot tests
 auto dot = [](const Vector3 &a, const Vector3 &b){ return  a.x*b.x + a.y*b.y + a.z*b.z; };
 if (!feq(dot(colX,colY),0.0f) || !feq(dot(colX,colZ),0.0f) || !feq(dot(colY,colZ),0.0f)) {
 std::cout << "Columns are not orthogonal\n";
 return 2;
 }
 if (!feq(dot(colX,colX),1.0f) || !feq(dot(colY,colY),1.0f) || !feq(dot(colZ,colZ),1.0f)) {
 std::cout << "Columns are not unit-length\n";
 return 3;
 }

 // determinant near +1
 float det = QM_MatrixDeterminant(r);
 if (!feq(det,1.0f,1e-3f)) {
 std::cout << "Rotation matrix determinant !=1 (det = " << det << ")\n";
 return 4;
 }

 // inverse == transpose for pure rotation
 Matrix inv = QM_MatrixInvert(r);
 Matrix t = QM_MatrixTranspose(r);
 // compare element-wise
 for (int i=0;i<16;i++) {
 float a = (&inv.m0)[i];
 float b = (&t.m0)[i];
 if (!feq(a,b,1e-3f)) {
 std::cout << "Inverse != Transpose for rotation matrix at index " << i << " : " << a << " vs " << b << "\n";
 return 5;
 }
 }

 std::cout << "Orthonormality & inverse==transpose tests passed.\n";
 return 0;
}

int test_lookat_manual_assembly() {
 std::cout << "Test QM_MatrixLookAt vs manual assembly...\n";
 Vector3 eye = {1.0f,2.0f,3.0f };
 Vector3 target = {4.0f,0.5f, -1.0f };
 Vector3 up = {0.0f,0.0f,1.0f }; // Z+ up

 Matrix mLook = QM_MatrixLookAt(eye, target, up);

 // compute normalized forward (vz)
 Vector3 vz = { eye.x - target.x, eye.y - target.y, eye.z - target.z };
 float len = sqrtf(vz.x*vz.x + vz.y*vz.y + vz.z*vz.z);
 if (len ==0.0f) len =1.0f;
 vz.x /= len; vz.y /= len; vz.z /= len;

 // Use authoritative MakeNormalVectors to compute right/up for this forward
 vec3_t forward_arr = { vz.x, vz.y, vz.z };
 vec3_t right_arr = {0.0f,0.0f,0.0f };
 vec3_t up_arr = {0.0f,0.0f,0.0f };

 MakeNormalVectors(forward_arr, right_arr, up_arr);

 Vector3 mnv_right = { right_arr[0], right_arr[1], right_arr[2] };
 Vector3 mnv_up = { up_arr[0], up_arr[1], up_arr[2] };

 // assemble matrix from MakeNormalVectors basis (right, up, forward)
 Matrix mm = {0 };
 mm.m0 = mnv_right.x; mm.m1 = mnv_up.x; mm.m2 = vz.x; mm.m3 =0.0f;
 mm.m4 = mnv_right.y; mm.m5 = mnv_up.y; mm.m6 = vz.y; mm.m7 =0.0f;
 mm.m8 = mnv_right.z; mm.m9 = mnv_up.z; mm.m10 = vz.z; mm.m11 =0.0f;
 mm.m12 = -( mnv_right.x * eye.x + mnv_right.y * eye.y + mnv_right.z * eye.z );
 mm.m13 = -( mnv_up.x * eye.x + mnv_up.y * eye.y + mnv_up.z * eye.z );
 mm.m14 = -( vz.x * eye.x + vz.y * eye.y + vz.z * eye.z );
 mm.m15 =1.0f;

 // Compare matrices
 if (!compareMatrices(mLook, mm)) {
 std::cout << "MakeNormalVectors-based assembly does not match QM_MatrixLookAt.\n";
 std::cout << "QM_MatrixLookAt first column (vx): [" << mLook.m0 << ", " << mLook.m4 << ", " << mLook.m8 << "]\n";
 std::cout << "MNV right first column: [" << mm.m0 << ", " << mm.m4 << ", " << mm.m8 << "]\n";
 std::cout << "QM_MatrixLookAt second column (vy): [" << mLook.m1 << ", " << mLook.m5 << ", " << mLook.m9 << "]\n";
 std::cout << "MNV up second column: [" << mm.m1 << ", " << mm.m5 << ", " << mm.m9 << "]\n";
 return 7;
 }

 std::cout << "LookAt vs MakeNormalVectors assembly match.\n";
 return 0;
}

int test_roundtrip_transform() {
 std::cout << "Test transform roundtrip (M * inv(M))...\n";
 Matrix r = QM_MatrixRotateXYZ( Vector3{0.2f, -0.5f,0.9f} );
 //r.m12 =1.0f; r.m13 = -2.0f; r.m14 =0.5f; // add translation
 r = QM_MatrixTranslate( r, Vector3{1.0f, -2.0f,0.5f } );
 Vector3 p = {0.3f,0.7f, -1.1f };

 Vector3 t = transformPoint(r, p);
 Matrix inv = QM_MatrixInvert(r);
 Vector3 back = transformPoint(inv, t);
 if (!checkVec(p, back,1e-3f)) {
 printVec("original", p); printVec("back", back);
 return 7;
 }

 std::cout << "Roundtrip transform passed.\n";
 return 0;
}

int main( int argc, char *argv[] ) {
 std::cout << std::fixed << std::setprecision(6);
 if (int rc = test_rotations()) return  rc;
 if (int rc = test_orthonormality_and_inverse_transpose()) return  rc;
 if (int rc = test_lookat_manual_assembly()) return  rc;
 if (int rc = test_roundtrip_transform()) return  rc;

 std::cout << "All matrix verification tests passed.\n";
 return 0;
}