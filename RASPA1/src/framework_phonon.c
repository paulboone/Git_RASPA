/*****************************************************************************************************
    RASPA: a molecular-dynamics, monte-carlo and optimization code for nanoporous materials
    Copyright (C) 2006-2012 David Dubbeldam, Sofia Calero, Donald E. Ellis, and Randall Q. Snurr.

    D.Dubbeldam@uva.nl            http://molsim.science.uva.nl/
    scaldia@upo.es                http://www.upo.es/raspa/
    don-ellis@northwestern.edu    http://dvworld.northwestern.edu/
    snurr@northwestern.edu        http://zeolites.cqe.northwestern.edu/

    This file 'framework_phonon.h' is part of RASPA.

    RASPA is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    RASPA is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "constants.h"
#include "molecule.h"
#include "framework.h"
#include "framework_energy.h"
#include "framework_force.h"
#include "simulation.h"
#include "ewald.h"
#include "potentials.h"
#include "utils.h"
#include "output.h"
#include "cbmc.h"
#include "mc_moves.h"
#include "grids.h"
#include "spacegroup.h"
#include "spectra.h"
#include "rigid.h"
#include "minimization.h"


// Hessian: Center of mass - Center of mass
// ========================================
static inline void HessianAtomicPositionPosition(COMPLEX phase_factor,COMPLEX_MATRIX HessianMatrix,int index_i,int index_j,REAL f1,REAL f2,VECTOR dr,REAL ReplicaFactor)
{
  REAL_MATRIX3x3 Hessian;

  Hessian.ax=f2*dr.x*dr.x+f1; Hessian.bx=f2*dr.y*dr.x;    Hessian.cx=f2*dr.z*dr.x;
  Hessian.ay=f2*dr.x*dr.y;    Hessian.by=f2*dr.y*dr.y+f1; Hessian.cy=f2*dr.z*dr.y;
  Hessian.az=f2*dr.x*dr.z;    Hessian.bz=f2*dr.y*dr.z;    Hessian.cz=f2*dr.z*dr.z+f1;

  if(Framework[CurrentSystem].FrameworkModel==FLEXIBLE)
  { 
    if(index_i>=0)
    {
      HessianMatrix.element[index_i][index_i].re+=ReplicaFactor*Hessian.ax;
      HessianMatrix.element[index_i][index_i+1].re+=ReplicaFactor*Hessian.ay;
      HessianMatrix.element[index_i][index_i+2].re+=ReplicaFactor*Hessian.az;
      HessianMatrix.element[index_i+1][index_i+1].re+=ReplicaFactor*Hessian.by;
      HessianMatrix.element[index_i+1][index_i+2].re+=ReplicaFactor*Hessian.bz;
      HessianMatrix.element[index_i+2][index_i+2].re+=ReplicaFactor*Hessian.cz;
    }
  }

  if(index_j>=0)
  { 
    HessianMatrix.element[index_j][index_j].re+=ReplicaFactor*Hessian.ax;
    HessianMatrix.element[index_j][index_j+1].re+=ReplicaFactor*Hessian.ay;
    HessianMatrix.element[index_j][index_j+2].re+=ReplicaFactor*Hessian.az;
    HessianMatrix.element[index_j+1][index_j+1].re+=ReplicaFactor*Hessian.by;
    HessianMatrix.element[index_j+1][index_j+2].re+=ReplicaFactor*Hessian.bz;
    HessianMatrix.element[index_j+2][index_j+2].re+=ReplicaFactor*Hessian.cz;
  }

  if(Framework[CurrentSystem].FrameworkModel==FLEXIBLE)
  {
    if((index_i>=0)&&(index_j>=0))
    {
      HessianMatrix.element[index_i][index_j].re-=phase_factor.re*Hessian.ax;
      HessianMatrix.element[index_i][index_j].im-=phase_factor.im*Hessian.ax;
      HessianMatrix.element[index_i][index_j+1].re-=phase_factor.re*Hessian.ay;
      HessianMatrix.element[index_i][index_j+1].im-=phase_factor.im*Hessian.ay;
      HessianMatrix.element[index_i][index_j+2].re-=phase_factor.re*Hessian.az;
      HessianMatrix.element[index_i][index_j+2].im-=phase_factor.im*Hessian.az;
      HessianMatrix.element[index_i+1][index_j].re-=phase_factor.re*Hessian.ay;
      HessianMatrix.element[index_i+1][index_j].im-=phase_factor.im*Hessian.ay;
      HessianMatrix.element[index_i+1][index_j+1].re-=phase_factor.re*Hessian.by;
      HessianMatrix.element[index_i+1][index_j+1].im-=phase_factor.im*Hessian.by;
      HessianMatrix.element[index_i+1][index_j+2].re-=phase_factor.re*Hessian.bz;
      HessianMatrix.element[index_i+1][index_j+2].im-=phase_factor.im*Hessian.bz;
      HessianMatrix.element[index_i+2][index_j].re-=phase_factor.re*Hessian.az;
      HessianMatrix.element[index_i+2][index_j].im-=phase_factor.im*Hessian.az;
      HessianMatrix.element[index_i+2][index_j+1].re-=phase_factor.re*Hessian.bz;
      HessianMatrix.element[index_i+2][index_j+1].im-=phase_factor.im*Hessian.bz;
      HessianMatrix.element[index_i+2][index_j+2].re-=phase_factor.re*Hessian.cz;
      HessianMatrix.element[index_i+2][index_j+2].im-=phase_factor.im*Hessian.cz;
    }
  }
}


// Hessian: Center of mass - Orientation
// =====================================
static inline void HessianCenterOfMassOrientationJ(COMPLEX_MATRIX HessianMatrix,int index_i,int index_j,int index_j2,int index2,REAL f1,REAL f2,VECTOR dr)
{
  REAL_MATRIX3x3 Hessian;
  VECTOR vecj1,vecj2,vecj3;

  vecj1=DVecX[index2];
  vecj2=DVecY[index2];
  vecj3=DVecZ[index2];

  Hessian.ax=f2*(dr.x*vecj1.x+dr.y*vecj1.y+dr.z*vecj1.z)*dr.x+f1*vecj1.x;
  Hessian.ay=f2*(dr.x*vecj2.x+dr.y*vecj2.y+dr.z*vecj2.z)*dr.x+f1*vecj2.x;
  Hessian.az=f2*(dr.x*vecj3.x+dr.y*vecj3.y+dr.z*vecj3.z)*dr.x+f1*vecj3.x;

  Hessian.bx=f2*(dr.x*vecj1.x+dr.y*vecj1.y+dr.z*vecj1.z)*dr.y+f1*vecj1.y;
  Hessian.by=f2*(dr.x*vecj2.x+dr.y*vecj2.y+dr.z*vecj2.z)*dr.y+f1*vecj2.y;
  Hessian.bz=f2*(dr.x*vecj3.x+dr.y*vecj3.y+dr.z*vecj3.z)*dr.y+f1*vecj3.y;

  Hessian.cx=f2*(dr.x*vecj1.x+dr.y*vecj1.y+dr.z*vecj1.z)*dr.z+f1*vecj1.z;
  Hessian.cy=f2*(dr.x*vecj2.x+dr.y*vecj2.y+dr.z*vecj2.z)*dr.z+f1*vecj2.z;
  Hessian.cz=f2*(dr.x*vecj3.x+dr.y*vecj3.y+dr.z*vecj3.z)*dr.z+f1*vecj3.z;

  if((index_j>=0)&&(index_j2>=0))
  {
    HessianMatrix.element[index_j][index_j2].re+=Hessian.ax;
    HessianMatrix.element[index_j][index_j2+1].re+=Hessian.ay;
    HessianMatrix.element[index_j][index_j2+2].re+=Hessian.az;

    HessianMatrix.element[index_j+1][index_j2].re+=Hessian.bx;
    HessianMatrix.element[index_j+1][index_j2+1].re+=Hessian.by;
    HessianMatrix.element[index_j+1][index_j2+2].re+=Hessian.bz;

    HessianMatrix.element[index_j+2][index_j2].re+=Hessian.cx;
    HessianMatrix.element[index_j+2][index_j2+1].re+=Hessian.cy;
    HessianMatrix.element[index_j+2][index_j2+2].re+=Hessian.cz;
  }

  Hessian.ax=f2*(dr.x*vecj1.x+dr.y*vecj1.y+dr.z*vecj1.z)*dr.x+f1*vecj1.x;
  Hessian.ay=f2*(dr.x*vecj2.x+dr.y*vecj2.y+dr.z*vecj2.z)*dr.x+f1*vecj2.x;
  Hessian.az=f2*(dr.x*vecj3.x+dr.y*vecj3.y+dr.z*vecj3.z)*dr.x+f1*vecj3.x;

  Hessian.bx=f2*(dr.x*vecj1.x+dr.y*vecj1.y+dr.z*vecj1.z)*dr.y+f1*vecj1.y;
  Hessian.by=f2*(dr.x*vecj2.x+dr.y*vecj2.y+dr.z*vecj2.z)*dr.y+f1*vecj2.y;
  Hessian.bz=f2*(dr.x*vecj3.x+dr.y*vecj3.y+dr.z*vecj3.z)*dr.y+f1*vecj3.y;

  Hessian.cx=f2*(dr.x*vecj1.x+dr.y*vecj1.y+dr.z*vecj1.z)*dr.z+f1*vecj1.z;
  Hessian.cy=f2*(dr.x*vecj2.x+dr.y*vecj2.y+dr.z*vecj2.z)*dr.z+f1*vecj2.z;
  Hessian.cz=f2*(dr.x*vecj3.x+dr.y*vecj3.y+dr.z*vecj3.z)*dr.z+f1*vecj3.z;

  if(Framework[CurrentSystem].FrameworkModel==FLEXIBLE)
  {
    if((index_i>=0)&&(index_j2>=0))
    {
      HessianMatrix.element[index_i][index_j2].re-=Hessian.ax;
      HessianMatrix.element[index_i][index_j2+1].re-=Hessian.ay;
      HessianMatrix.element[index_i][index_j2+2].re-=Hessian.az;

      HessianMatrix.element[index_i+1][index_j2].re-=Hessian.bx;
      HessianMatrix.element[index_i+1][index_j2+1].re-=Hessian.by;
      HessianMatrix.element[index_i+1][index_j2+2].re-=Hessian.bz;

      HessianMatrix.element[index_i+2][index_j2].re-=Hessian.cx;
      HessianMatrix.element[index_i+2][index_j2+1].re-=Hessian.cy;
      HessianMatrix.element[index_i+2][index_j2+2].re-=Hessian.cz;
    }
  }
}

// Hessian: Orientation - Orientation
// ==================================
static inline void HessianOrientationOrientationJ(COMPLEX_MATRIX HessianMatrix,int index_j2,int index2,REAL f1,REAL f2,VECTOR dr)
{
  REAL_MATRIX3x3 Hessian;
  VECTOR vecj1,vecj2,vecj3;
  VECTOR DDvecJAX,DDvecJBY,DDvecJCZ,DDvecJAY,DDvecJAZ,DDvecJBZ;

  vecj1=DVecX[index2];
  vecj2=DVecY[index2];
  vecj3=DVecZ[index2];

  DDvecJAX=DDVecAX[index2];
  DDvecJBY=DDVecBY[index2];
  DDvecJCZ=DDVecCZ[index2];
  DDvecJAY=DDVecAY[index2];
  DDvecJAZ=DDVecAZ[index2];
  DDvecJBZ=DDVecBZ[index2];

  if(index_j2>=0)
  {
    Hessian.ax=-f1*(dr.x*DDvecJAX.x+dr.y*DDvecJAX.y+dr.z*DDvecJAX.z)
               +f2*(dr.x*vecj1.x+dr.y*vecj1.y+dr.z*vecj1.z)*(dr.x*vecj1.x+dr.y*vecj1.y+dr.z*vecj1.z)
               +f1*(vecj1.x*vecj1.x+vecj1.y*vecj1.y+vecj1.z*vecj1.z);

    Hessian.by=-f1*(dr.x*DDvecJBY.x+dr.y*DDvecJBY.y+dr.z*DDvecJBY.z)+
               f2*(dr.x*vecj2.x+dr.y*vecj2.y+dr.z*vecj2.z)*(dr.x*vecj2.x+dr.y*vecj2.y+dr.z*vecj2.z)+
               f1*(vecj2.x*vecj2.x+vecj2.y*vecj2.y+vecj2.z*vecj2.z);

    Hessian.cz=-f1*(dr.x*DDvecJCZ.x+dr.y*DDvecJCZ.y+dr.z*DDvecJCZ.z)+
               f2*(dr.x*vecj3.x+dr.y*vecj3.y+dr.z*vecj3.z)*(dr.x*vecj3.x+dr.y*vecj3.y+dr.z*vecj3.z)+
               f1*(vecj3.x*vecj3.x+vecj3.y*vecj3.y+vecj3.z*vecj3.z);

    Hessian.ay=-f1*(dr.x*DDvecJAY.x+dr.y*DDvecJAY.y+dr.z*DDvecJAY.z)+
               f2*(dr.x*vecj1.x+dr.y*vecj1.y+dr.z*vecj1.z)*(dr.x*vecj2.x+dr.y*vecj2.y+dr.z*vecj2.z)+
               f1*(vecj1.x*vecj2.x+vecj1.y*vecj2.y+vecj1.z*vecj2.z);

    Hessian.az=-f1*(dr.x*DDvecJAZ.x+dr.y*DDvecJAZ.y+dr.z*DDvecJAZ.z)+
               f2*(dr.x*vecj1.x+dr.y*vecj1.y+dr.z*vecj1.z)*(dr.x*vecj3.x+dr.y*vecj3.y+dr.z*vecj3.z)+
               f1*(vecj1.x*vecj3.x+vecj1.y*vecj3.y+vecj1.z*vecj3.z);

    Hessian.bz=-f1*(dr.x*DDvecJBZ.x+dr.y*DDvecJBZ.y+dr.z*DDvecJBZ.z)+
               f2*(dr.x*vecj2.x+dr.y*vecj2.y+dr.z*vecj2.z)*(dr.x*vecj3.x+dr.y*vecj3.y+dr.z*vecj3.z)+
               f1*(vecj2.x*vecj3.x+vecj2.y*vecj3.y+vecj2.z*vecj3.z);

    HessianMatrix.element[index_j2][index_j2].re+=Hessian.ax;
    HessianMatrix.element[index_j2+1][index_j2+1].re+=Hessian.by;
    HessianMatrix.element[index_j2+2][index_j2+2].re+=Hessian.cz;
    HessianMatrix.element[index_j2][index_j2+1].re+=Hessian.ay;
    HessianMatrix.element[index_j2][index_j2+2].re+=Hessian.az;
    HessianMatrix.element[index_j2+1][index_j2+2].re+=Hessian.bz;
  }
}

static inline void GradientStrain(REAL *Gradient,REAL f1,VECTOR dr)
{
  int n;

  n=NumberOfCoordinatesMinimizationVariables;
  switch(Ensemble[CurrentSystem])
  {
    case NPT:
      Gradient[n]+=f1*(dr.x*dr.x+dr.y*dr.y+dr.z*dr.z); // xx + yy + zz
      break;
    case NPTPR:
    case NPHPR:
      switch(NPTPRCellType[CurrentSystem])
      {
        case ISOTROPIC:
        case ANISOTROPIC:
          Gradient[n  ]+=f1*dr.x*dr.x; // xx
          Gradient[n+1]+=f1*dr.y*dr.y; // yy
          Gradient[n+2]+=f1*dr.z*dr.z; // zz
          break;
        case REGULAR:
        case REGULAR_UPPER_TRIANGLE:
          Gradient[n  ]+=f1*dr.x*dr.x;  // xx
          Gradient[n+1]+=f1*dr.x*dr.y;  // xy
          Gradient[n+2]+=f1*dr.x*dr.z;  // xz
          Gradient[n+3]+=f1*dr.y*dr.y;  // yy
          Gradient[n+4]+=f1*dr.y*dr.z;  // yz
          Gradient[n+5]+=f1*dr.z*dr.z;  // zz
          break;
        case MONOCLINIC:
        case MONOCLINIC_UPPER_TRIANGLE:
          switch(MonoclinicAngleType[CurrentSystem])
          {
            case MONOCLINIC_ALPHA_ANGLE:
              Gradient[n  ]+=f1*dr.x*dr.x;  // xx
              Gradient[n+1]+=f1*dr.y*dr.y;  // yy
              Gradient[n+2]+=f1*dr.y*dr.z;  // yz
              Gradient[n+3]+=f1*dr.z*dr.z;  // zz
              break;
            case MONOCLINIC_BETA_ANGLE:
              Gradient[n  ]+=f1*dr.x*dr.x;  // xx
              Gradient[n+1]+=f1*dr.x*dr.z;  // xz
              Gradient[n+2]+=f1*dr.y*dr.y;  // yy
              Gradient[n+3]+=f1*dr.z*dr.z;  // zz
              break;
            case MONOCLINIC_GAMMA_ANGLE:
              Gradient[n  ]+=f1*dr.x*dr.x;  // xx
              Gradient[n+1]+=f1*dr.x*dr.y;  // xy
              Gradient[n+2]+=f1*dr.y*dr.y;  // yy
              Gradient[n+3]+=f1*dr.z*dr.z;  // zz
              break;
          }
          break;
        default:
          printf("Unknown NPTPRCellType\n");
          exit(0);
          break;
      }
      break;
    case NVT:
    case NVE:
      break;
  }

}

static inline void GradientStrainJ(REAL *Gradient,REAL f1,VECTOR dr,VECTOR posB,VECTOR comB)
{
  int n;

  n=NumberOfCoordinatesMinimizationVariables;
  switch(Ensemble[CurrentSystem])
  {
    case NPT:
      Gradient[n  ]+=f1*(dr.x*(posB.x-comB.x)+dr.y*(posB.y-comB.y)+dr.z*(posB.z-comB.z)); // xx + yy + zz
      break;
    case NPTPR:
    case NPHPR:
      switch(NPTPRCellType[CurrentSystem])
      {
        case ISOTROPIC:
        case ANISOTROPIC:
          Gradient[n  ]+=f1*dr.x*(posB.x-comB.x); // xx
          Gradient[n+1]+=f1*dr.y*(posB.y-comB.y); // yy
          Gradient[n+2]+=f1*dr.z*(posB.z-comB.z); // zz
          break;
        case REGULAR:
          Gradient[n  ]+=f1*dr.x*(posB.x-comB.x);                            // xx
          Gradient[n+1]+=f1*0.5*(dr.x*(posB.y-comB.y)+dr.y*(posB.x-comB.x)); // xy
          Gradient[n+2]+=f1*0.5*(dr.x*(posB.z-comB.z)+dr.z*(posB.x-comB.x)); // xz
          Gradient[n+3]+=f1*(posB.y-comB.y)*dr.y;                            // yy
          Gradient[n+4]+=f1*0.5*(dr.y*(posB.z-comB.z)+dr.z*(posB.y-comB.y)); // yz
          Gradient[n+5]+=f1*dr.z*(posB.z-comB.z);                            // zz
          break;
        case REGULAR_UPPER_TRIANGLE:
          Gradient[n  ]+=f1*dr.x*(posB.x-comB.x); // xx
          Gradient[n+1]+=f1*dr.x*(posB.y-comB.y); // xy
          Gradient[n+2]+=f1*dr.x*(posB.z-comB.z); // xz
          Gradient[n+3]+=f1*dr.y*(posB.y-comB.y); // yy
          Gradient[n+4]+=f1*dr.y*(posB.z-comB.z); // yz
          Gradient[n+5]+=f1*dr.z*(posB.z-comB.z); // zz
          break;
        case MONOCLINIC:
          switch(MonoclinicAngleType[CurrentSystem])
          {
            case MONOCLINIC_ALPHA_ANGLE:
              Gradient[n  ]+=f1*dr.x*(posB.x-comB.x);                            // xx
              Gradient[n+1]+=f1*(posB.y-comB.y)*dr.y;                            // yy
              Gradient[n+2]+=f1*0.5*(dr.y*(posB.z-comB.z)+dr.z*(posB.y-comB.y)); // yz
              Gradient[n+3]+=f1*dr.z*(posB.z-comB.z);                            // zz
              break;
            case MONOCLINIC_BETA_ANGLE:
              Gradient[n  ]+=f1*dr.x*(posB.x-comB.x);                            // xx
              Gradient[n+1]+=f1*0.5*(dr.x*(posB.z-comB.z)+dr.z*(posB.x-comB.x)); // xz
              Gradient[n+2]+=f1*(posB.y-comB.y)*dr.y;                            // yy
              Gradient[n+3]+=f1*dr.z*(posB.z-comB.z);                            // zz
              break;
            case MONOCLINIC_GAMMA_ANGLE:
              Gradient[n  ]+=f1*dr.x*(posB.x-comB.x);                            // xx
              Gradient[n+1]+=f1*0.5*(dr.x*(posB.y-comB.y)+dr.y*(posB.x-comB.x)); // xy
              Gradient[n+2]+=f1*(posB.y-comB.y)*dr.y;                            // yy
              Gradient[n+3]+=f1*dr.z*(posB.z-comB.z);                            // zz
              break;
          }
          break;
        case MONOCLINIC_UPPER_TRIANGLE:
          switch(MonoclinicAngleType[CurrentSystem])
          {
            case MONOCLINIC_ALPHA_ANGLE:
              Gradient[n  ]+=f1*dr.x*(posB.x-comB.x); // xx
              Gradient[n+1]+=f1*dr.y*(posB.y-comB.y); // yy
              Gradient[n+2]+=f1*dr.y*(posB.z-comB.z); // yz
              Gradient[n+3]+=f1*dr.z*(posB.z-comB.z); // zz
              break;
            case MONOCLINIC_BETA_ANGLE:
              Gradient[n  ]+=f1*dr.x*(posB.x-comB.x); // xx
              Gradient[n+1]+=f1*dr.x*(posB.z-comB.z); // xz
              Gradient[n+2]+=f1*dr.y*(posB.y-comB.y); // yy
              Gradient[n+3]+=f1*dr.z*(posB.z-comB.z); // zz
              break;
            case MONOCLINIC_GAMMA_ANGLE:
              Gradient[n  ]+=f1*dr.x*(posB.x-comB.x); // xx
              Gradient[n+1]+=f1*dr.x*(posB.y-comB.y); // xy
              Gradient[n+2]+=f1*dr.y*(posB.y-comB.y); // yy
              Gradient[n+3]+=f1*dr.z*(posB.z-comB.z); // zz
              break;
          }
          break;
        default:
          printf("Unknown NPTPRCellType\n");
          exit(0);
          break;
      }
      break;
    case NVT:
    case NVE:
      break;
  }
}

void ComputeFrameworkAdsorbateVDWPhonon(VECTOR k,REAL *Energy,REAL* Gradient,COMPLEX_MATRIX HessianMatrix,
                                         REAL_MATRIX3x3 *StrainDerivative,int ComputeGradient,int ComputeHessian)
{
  int J,i,j,jg,ja,fr;
  int typeA,typeB;
  int TypeMolB;
  REAL rr;
  REAL energy,f1,f2;
  VECTOR posA,posB,dr;
  int index_i,index_j,index_j2;
  int index2;
  REAL ChargeA,ChargeB;
  VECTOR pos,comB;
  REAL r,temp1,temp2,temp3;
  int ncell,k1,k2,k3;
  COMPLEX phase_factor;

  f1=f2=0.0;
  index2=0;
  // first loop over adsorbate molecules

  for(fr=0;fr<Framework[CurrentSystem].NumberOfFrameworks;fr++)
  {
    for(i=0;i<Framework[CurrentSystem].NumberOfAtoms[fr];i++)
    {
      index_i=Framework[CurrentSystem].Atoms[fr][i].HessianIndex;
      posA=Framework[CurrentSystem].Atoms[fr][i].Position;
      typeA=Framework[CurrentSystem].Atoms[fr][i].Type;
      ChargeA=Framework[CurrentSystem].Atoms[fr][i].Charge;

        // second loop over adsorbates
      ncell=0;
      for(k1=0;k1<NumberOfReplicaCells[CurrentSystem].x;k1++)
        for(k2=0;k2<NumberOfReplicaCells[CurrentSystem].y;k2++)
          for(k3=0;k3<NumberOfReplicaCells[CurrentSystem].z;k3++)
          {
            for(J=0;J<NumberOfAdsorbateMolecules[CurrentSystem];J++)
            {
              TypeMolB=Adsorbates[CurrentSystem][J].Type;
              for(jg=0;jg<Components[TypeMolB].NumberOfGroups;jg++)
              {
                for(ja=0;ja<Components[TypeMolB].Groups[jg].NumberOfGroupAtoms;ja++)
                {
                  j=Components[TypeMolB].Groups[jg].Atoms[ja];
 
                  if(Components[TypeMolB].Groups[jg].Rigid)
                  {
                    index_j=Adsorbates[CurrentSystem][J].Groups[jg].HessianIndex;
                    index_j2=Adsorbates[CurrentSystem][J].Groups[jg].HessianIndexOrientation;

                    index2=Adsorbates[CurrentSystem][J].Atoms[j].HessianAtomIndex;
                  }
                  else
                  {
                    index_j=Adsorbates[CurrentSystem][J].Atoms[j].HessianIndex;
                    index_j2=-1;
                  }

                  typeB=Adsorbates[CurrentSystem][J].Atoms[j].Type;
                  posB=Adsorbates[CurrentSystem][J].Atoms[j].Position;
                  ChargeB=Adsorbates[CurrentSystem][J].Atoms[j].Charge;

                  posB.x+=ReplicaShift[ncell].x;
                  posB.y+=ReplicaShift[ncell].y;
                  posB.z+=ReplicaShift[ncell].z;

                  dr.x=posA.x-posB.x;
                  dr.y=posA.y-posB.y;
                  dr.z=posA.z-posB.z;
                  dr=ApplyReplicaBoundaryCondition(dr);
                  rr=SQR(dr.x)+SQR(dr.y)+SQR(dr.z);
                  r=sqrt(rr);

                  if(rr<CutOffVDWSquared)
                  {
                    PotentialSecondDerivative(typeA,typeB,rr,&energy,&f1,&f2);

                    // add contribution to the energy
                    *Energy+=energy;

                    if((index_i<0)&&(index_j<0)&&(index_j2<0)) continue;

                    StrainDerivative->ax+=f1*dr.x*dr.x;
                    StrainDerivative->bx+=f1*dr.x*dr.y;
                    StrainDerivative->cx+=f1*dr.x*dr.z;

                    StrainDerivative->ay+=f1*dr.y*dr.x;
                    StrainDerivative->by+=f1*dr.y*dr.y;
                    StrainDerivative->cy+=f1*dr.y*dr.z;

                    StrainDerivative->az+=f1*dr.z*dr.x;
                    StrainDerivative->bz+=f1*dr.z*dr.y;
                    StrainDerivative->cz+=f1*dr.z*dr.z;

                    if(Components[TypeMolB].Groups[jg].Rigid)
                    {
                      comB=Adsorbates[CurrentSystem][J].Groups[jg].CenterOfMassPosition;
                      comB.x+=ReplicaShift[ncell].x;
                      comB.y+=ReplicaShift[ncell].y;
                      comB.z+=ReplicaShift[ncell].z;

                      pos=Components[TypeMolB].Positions[j];

                      temp1=0.5*((posB.y-comB.y)*f1*dr.x+(posB.x-comB.x)*f1*dr.y);
                      temp2=0.5*((posB.z-comB.z)*f1*dr.x+(posB.x-comB.x)*f1*dr.z);
                      temp3=0.5*((posB.z-comB.z)*f1*dr.y+(posB.y-comB.y)*f1*dr.z);
                      StrainDerivative->ax+=(posB.x-comB.x)*f1*dr.x;
                      StrainDerivative->ay+=temp1;
                      StrainDerivative->az+=temp2;
                      StrainDerivative->bx+=temp1;
                      StrainDerivative->by+=(posB.y-comB.y)*f1*dr.y;
                      StrainDerivative->bz+=temp3;
                      StrainDerivative->cx+=temp2;
                      StrainDerivative->cy+=temp3;
                      StrainDerivative->cz+=(posB.z-comB.z)*f1*dr.z;
                    }

                    // add contribution to the first derivatives
                    if(ComputeGradient)
                    {
                      if(Framework[CurrentSystem].FrameworkModel==FLEXIBLE)
                      {
                        if(index_i>=0)
                        {
                          Gradient[index_i]+=f1*dr.x;
                          Gradient[index_i+1]+=f1*dr.y;
                          Gradient[index_i+2]+=f1*dr.z;
                        }
                      }
  
                      if(index_j>=0)
                      {
                        Gradient[index_j]-=f1*dr.x;
                        Gradient[index_j+1]-=f1*dr.y;
                        Gradient[index_j+2]-=f1*dr.z;
                      }

                      GradientStrain(Gradient,f1,dr);

                      if(Components[TypeMolB].Groups[jg].Rigid)
                      {
                        GradientStrainJ(Gradient,f1,dr,posB,comB);

                        // add contribution to the first derivatives
                        if(index_j2>=0)
                        {
                          Gradient[index_j2]-=f1*(dr.x*DVecX[index2].x+dr.y*DVecX[index2].y+dr.z*DVecX[index2].z);
                          Gradient[index_j2+1]-=f1*(dr.x*DVecY[index2].x+dr.y*DVecY[index2].y+dr.z*DVecY[index2].z);
                          Gradient[index_j2+2]-=f1*(dr.x*DVecZ[index2].x+dr.y*DVecZ[index2].y+dr.z*DVecZ[index2].z);
                        }
                      }
                    }

                    if(ComputeHessian)
                    {
                      HessianAtomicPositionPosition(phase_factor,HessianMatrix,index_i,index_j,f1,f2,dr,1.0);

                      if(Components[TypeMolB].Groups[jg].Rigid)
                      {
                        HessianCenterOfMassOrientationJ(HessianMatrix,index_i,index_j,index_j2,index2,f1,f2,dr);
                        HessianOrientationOrientationJ(HessianMatrix,index_j2,index2,f1,f2,dr);
                      }
                    } 
                  }
                }
              }
            }
            ncell++;
          }
    }
  }
}

void ComputeFrameworkAdsorbateChargeChargePhonon(VECTOR k,REAL *Energy,REAL* Gradient,COMPLEX_MATRIX HessianMatrix,
                                                  REAL_MATRIX3x3 *StrainDerivative,int ComputeGradient,int ComputeHessian)
{
  int J,i,j,jg,ja,fr;
  int typeA,typeB;
  int TypeMolB;
  REAL rr;
  REAL f1,f2;
  VECTOR posA,posB,dr;
  int index_i,index_j,index_j2;
  int index2;
  REAL ChargeA,ChargeB;
  VECTOR comB;
  REAL r,temp1,temp2,temp3;
  int ncell,k1,k2,k3;
  COMPLEX phase_factor;

  f1=f2=0.0;
  index2=0;
  // first loop over adsorbate molecules
  for(fr=0;fr<Framework[CurrentSystem].NumberOfFrameworks;fr++)
  {
    for(i=0;i<Framework[CurrentSystem].NumberOfAtoms[fr];i++)
    {
      index_i=Framework[CurrentSystem].Atoms[fr][i].HessianIndex;
      posA=Framework[CurrentSystem].Atoms[fr][i].Position;
      typeA=Framework[CurrentSystem].Atoms[fr][i].Type;
      ChargeA=Framework[CurrentSystem].Atoms[fr][i].Charge;

      // second loop over adsorbates
      ncell=0;
      for(k1=0;k1<NumberOfReplicaCells[CurrentSystem].x;k1++)
        for(k2=0;k2<NumberOfReplicaCells[CurrentSystem].y;k2++)
          for(k3=0;k3<NumberOfReplicaCells[CurrentSystem].z;k3++)
          {
            for(J=0;J<NumberOfAdsorbateMolecules[CurrentSystem];J++)
            {
              TypeMolB=Adsorbates[CurrentSystem][J].Type;
              for(jg=0;jg<Components[TypeMolB].NumberOfGroups;jg++)
              {
                for(ja=0;ja<Components[TypeMolB].Groups[jg].NumberOfGroupAtoms;ja++)
                {
                  j=Components[TypeMolB].Groups[jg].Atoms[ja];

                  if(Components[TypeMolB].Groups[jg].Rigid)
                  {
                    index_j=Adsorbates[CurrentSystem][J].Groups[jg].HessianIndex;
                    index_j2=Adsorbates[CurrentSystem][J].Groups[jg].HessianIndexOrientation;

                    index2=Adsorbates[CurrentSystem][J].Atoms[j].HessianAtomIndex;
                  }
                  else
                  {
                    index_j=Adsorbates[CurrentSystem][J].Atoms[j].HessianIndex;
                    index_j2=-1;
                  }

                  typeB=Adsorbates[CurrentSystem][J].Atoms[j].Type;
                  posB=Adsorbates[CurrentSystem][J].Atoms[j].Position;
                  ChargeB=Adsorbates[CurrentSystem][J].Atoms[j].Charge;

                  posB.x+=ReplicaShift[ncell].x;
                  posB.y+=ReplicaShift[ncell].y;
                  posB.z+=ReplicaShift[ncell].z;

                  dr.x=posA.x-posB.x;
                  dr.y=posA.y-posB.y;
                  dr.z=posA.z-posB.z;
                  dr=ApplyReplicaBoundaryCondition(dr);
                  rr=SQR(dr.x)+SQR(dr.y)+SQR(dr.z);
                  r=sqrt(rr);

                  if(rr<CutOffChargeChargeSquared)
                  {
                    switch(ChargeMethod)
                    {
                      case NONE:
                        f1=f2=0.0;
                        break;
                      case SHIFTED_COULOMB:
                        *Energy+=COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB*(1.0/r-InverseCutOffChargeCharge);
                        f1=-COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB/(rr*r);
                        f2=3.0*COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB/(SQR(rr)*r);
                        break;
                      case TRUNCATED_COULOMB:
                        *Energy+=COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB*(1.0/r);
                        f1=-COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB/(rr*r);
                        f2=3.0*COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB/(SQR(rr)*r);
                        break;
                      case EWALD:
                      default:
                        *Energy+=COULOMBIC_CONVERSION_FACTOR*erfc(Alpha[CurrentSystem]*r)*
                                                ChargeA*ChargeB/r;
                        f1=-COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB*
                            (erfc(Alpha[CurrentSystem]*r)+2.0*Alpha[CurrentSystem]*r*exp(-SQR(Alpha[CurrentSystem])*rr)/sqrt(M_PI))/
                            (r*rr);

                        f2=ChargeA*ChargeB*COULOMBIC_CONVERSION_FACTOR*
                           (((3.0*erfc(Alpha[CurrentSystem]*r)/(r*rr))+
                           (4.0*CUBE(Alpha[CurrentSystem])*exp(-SQR(Alpha[CurrentSystem])*rr)/sqrt(M_PI))+
                           (6.0*Alpha[CurrentSystem]*exp(-SQR(Alpha[CurrentSystem])*rr)/(sqrt(M_PI)*rr)))/(rr));
                        break;
                    }

                    if((index_i<0)&&(index_j<0)) continue;

                    StrainDerivative->ax+=f1*dr.x*dr.x;
                    StrainDerivative->bx+=f1*dr.x*dr.y;
                    StrainDerivative->cx+=f1*dr.x*dr.z;

                    StrainDerivative->ay+=f1*dr.y*dr.x;
                    StrainDerivative->by+=f1*dr.y*dr.y;
                    StrainDerivative->cy+=f1*dr.y*dr.z;

                    StrainDerivative->az+=f1*dr.z*dr.x;
                    StrainDerivative->bz+=f1*dr.z*dr.y;
                    StrainDerivative->cz+=f1*dr.z*dr.z;

                    if(Components[TypeMolB].Groups[jg].Rigid)
                    {
                      comB=Adsorbates[CurrentSystem][J].Groups[jg].CenterOfMassPosition;
                      comB.x+=ReplicaShift[ncell].x;
                      comB.y+=ReplicaShift[ncell].y;
                      comB.z+=ReplicaShift[ncell].z;

                      temp1=0.5*((posB.y-comB.y)*f1*dr.x+(posB.x-comB.x)*f1*dr.y);
                      temp2=0.5*((posB.z-comB.z)*f1*dr.x+(posB.x-comB.x)*f1*dr.z);
                      temp3=0.5*((posB.z-comB.z)*f1*dr.y+(posB.y-comB.y)*f1*dr.z);
                      StrainDerivative->ax+=(posB.x-comB.x)*f1*dr.x;
                      StrainDerivative->ay+=temp1;
                      StrainDerivative->az+=temp2;
                      StrainDerivative->bx+=temp1;
                      StrainDerivative->by+=(posB.y-comB.y)*f1*dr.y;
                      StrainDerivative->bz+=temp3;
                      StrainDerivative->cx+=temp2;
                      StrainDerivative->cy+=temp3;
                      StrainDerivative->cz+=(posB.z-comB.z)*f1*dr.z;
                    }

                    // add contribution to the first derivatives
                    if(ComputeGradient)
                    {
                      if(Framework[CurrentSystem].FrameworkModel==FLEXIBLE)
                      {
                        if(index_i>=0)
                        {
                          Gradient[index_i]+=f1*dr.x;
                          Gradient[index_i+1]+=f1*dr.y;
                          Gradient[index_i+2]+=f1*dr.z;
                        }
                      }

                      if(index_j>=0)
                      {
                        Gradient[index_j]-=f1*dr.x;
                        Gradient[index_j+1]-=f1*dr.y;
                        Gradient[index_j+2]-=f1*dr.z;
                      }
  
                      GradientStrain(Gradient,f1,dr);

                      if(Components[TypeMolB].Groups[jg].Rigid)
                      {
                        GradientStrainJ(Gradient,f1,dr,posB,comB);

                         // add contribution to the first derivatives
                        if(index_j2>=0)
                        {
                          Gradient[index_j2]-=f1*(dr.x*DVecX[index2].x+dr.y*DVecX[index2].y+dr.z*DVecX[index2].z);
                          Gradient[index_j2+1]-=f1*(dr.x*DVecY[index2].x+dr.y*DVecY[index2].y+dr.z*DVecY[index2].z);
                          Gradient[index_j2+2]-=f1*(dr.x*DVecZ[index2].x+dr.y*DVecZ[index2].y+dr.z*DVecZ[index2].z);
                        }
                      }
                    }

                    if(ComputeHessian)
                    {
                      HessianAtomicPositionPosition(phase_factor,HessianMatrix,index_i,index_j,f1,f2,dr,1.0);

                      if(Components[TypeMolB].Groups[jg].Rigid)
                      {
                        HessianCenterOfMassOrientationJ(HessianMatrix,index_i,index_j,index_j2,index2,f1,f2,dr);
                        HessianOrientationOrientationJ(HessianMatrix,index_j2,index2,f1,f2,dr);
                      }
                    }
                  }
                }
              }
            }
            ncell++;
          }
    }
  }
}

void ComputeFrameworkCationVDWPhonon(VECTOR k,REAL *Energy,REAL* Gradient,COMPLEX_MATRIX HessianMatrix,
                                      REAL_MATRIX3x3 *StrainDerivative,int ComputeGradient,int ComputeHessian)
{
  int J,i,j,jg,ja,fr;
  int typeA,typeB;
  int TypeMolB;
  REAL rr;
  REAL energy,f1,f2;
  VECTOR posA,posB,dr;
  int index_i,index_j,index_j2;
  int index2;
  REAL ChargeA,ChargeB;
  VECTOR comB;
  REAL r,temp1,temp2,temp3;
  int ncell,k1,k2,k3;
  COMPLEX phase_factor;

  f1=f2=0.0;
  index2=0;
  // first loop over adsorbate molecules

  for(fr=0;fr<Framework[CurrentSystem].NumberOfFrameworks;fr++)
  {
    for(i=0;i<Framework[CurrentSystem].NumberOfAtoms[fr];i++)
    {
      index_i=Framework[CurrentSystem].Atoms[fr][i].HessianIndex;
      posA=Framework[CurrentSystem].Atoms[fr][i].Position;
      typeA=Framework[CurrentSystem].Atoms[fr][i].Type;
      ChargeA=Framework[CurrentSystem].Atoms[fr][i].Charge;

      ncell=0;
      for(k1=0;k1<NumberOfReplicaCells[CurrentSystem].x;k1++)
        for(k2=0;k2<NumberOfReplicaCells[CurrentSystem].y;k2++)
          for(k3=0;k3<NumberOfReplicaCells[CurrentSystem].z;k3++)
          {
            for(J=0;J<NumberOfCationMolecules[CurrentSystem];J++)
            {
              TypeMolB=Cations[CurrentSystem][J].Type;
              for(jg=0;jg<Components[TypeMolB].NumberOfGroups;jg++)
              {
                for(ja=0;ja<Components[TypeMolB].Groups[jg].NumberOfGroupAtoms;ja++)
                {
                  j=Components[TypeMolB].Groups[jg].Atoms[ja];

                  if(Components[TypeMolB].Groups[jg].Rigid)
                  {
                    index_j=Cations[CurrentSystem][J].Groups[jg].HessianIndex;
                    index_j2=Cations[CurrentSystem][J].Groups[jg].HessianIndexOrientation;

                    index2=Cations[CurrentSystem][J].Atoms[j].HessianAtomIndex;
                  }
                  else
                  {
                    index_j=Cations[CurrentSystem][J].Atoms[j].HessianIndex;
                    index_j2=-1;
                  }

                  typeB=Cations[CurrentSystem][J].Atoms[j].Type;
                  posB=Cations[CurrentSystem][J].Atoms[j].Position;
                  ChargeB=Cations[CurrentSystem][J].Atoms[j].Charge;

                  posB.x+=ReplicaShift[ncell].x;
                  posB.y+=ReplicaShift[ncell].y;
                  posB.z+=ReplicaShift[ncell].z;

                  dr.x=posA.x-posB.x;
                  dr.y=posA.y-posB.y;
                  dr.z=posA.z-posB.z;
                  dr=ApplyReplicaBoundaryCondition(dr);
                  rr=SQR(dr.x)+SQR(dr.y)+SQR(dr.z);
                  r=sqrt(rr);

                  if(rr<CutOffVDWSquared)
                  {
                    PotentialSecondDerivative(typeA,typeB,rr,&energy,&f1,&f2);

                    // add contribution to the energy
                    *Energy+=energy;

                    StrainDerivative->ax+=f1*dr.x*dr.x;
                    StrainDerivative->bx+=f1*dr.x*dr.y;
                    StrainDerivative->cx+=f1*dr.x*dr.z;

                    StrainDerivative->ay+=f1*dr.y*dr.x;
                    StrainDerivative->by+=f1*dr.y*dr.y;
                    StrainDerivative->cy+=f1*dr.y*dr.z;

                    StrainDerivative->az+=f1*dr.z*dr.x;
                    StrainDerivative->bz+=f1*dr.z*dr.y;
                    StrainDerivative->cz+=f1*dr.z*dr.z;

                    if(Components[TypeMolB].Groups[jg].Rigid)
                    {
                      comB=Cations[CurrentSystem][J].Groups[jg].CenterOfMassPosition;
                      comB.x+=ReplicaShift[ncell].x;
                      comB.y+=ReplicaShift[ncell].y;
                      comB.z+=ReplicaShift[ncell].z;

                      temp1=0.5*((posB.y-comB.y)*f1*dr.x+(posB.x-comB.x)*f1*dr.y);
                      temp2=0.5*((posB.z-comB.z)*f1*dr.x+(posB.x-comB.x)*f1*dr.z);
                      temp3=0.5*((posB.z-comB.z)*f1*dr.y+(posB.y-comB.y)*f1*dr.z);
                      StrainDerivative->ax+=(posB.x-comB.x)*f1*dr.x;
                      StrainDerivative->ay+=temp1;
                      StrainDerivative->az+=temp2;
                      StrainDerivative->bx+=temp1;
                      StrainDerivative->by+=(posB.y-comB.y)*f1*dr.y;
                      StrainDerivative->bz+=temp3;
                      StrainDerivative->cx+=temp2;
                      StrainDerivative->cy+=temp3;
                      StrainDerivative->cz+=(posB.z-comB.z)*f1*dr.z;
                    }

                    // add contribution to the first derivatives
                    if(ComputeGradient)
                    {
                      if(Framework[CurrentSystem].FrameworkModel==FLEXIBLE)
                      {
                        if(index_i>=0)
                        {
                          Gradient[index_i]+=f1*dr.x;
                          Gradient[index_i+1]+=f1*dr.y;
                          Gradient[index_i+2]+=f1*dr.z;
                        }
                      }

                      if(index_j>=0)
                      {
                        Gradient[index_j]-=f1*dr.x;
                        Gradient[index_j+1]-=f1*dr.y;
                        Gradient[index_j+2]-=f1*dr.z;
                      }

                      GradientStrain(Gradient,f1,dr);

                      if(Components[TypeMolB].Groups[jg].Rigid)
                      {
                        GradientStrainJ(Gradient,f1,dr,posB,comB);

                        // add contribution to the first derivatives
                        if(index_j2>=0)
                        {
                          Gradient[index_j2]-=f1*(dr.x*DVecX[index2].x+dr.y*DVecX[index2].y+dr.z*DVecX[index2].z);
                          Gradient[index_j2+1]-=f1*(dr.x*DVecY[index2].x+dr.y*DVecY[index2].y+dr.z*DVecY[index2].z);
                          Gradient[index_j2+2]-=f1*(dr.x*DVecZ[index2].x+dr.y*DVecZ[index2].y+dr.z*DVecZ[index2].z);
                        }
                      }
                    }

                    if(ComputeHessian)
                    {
                      HessianAtomicPositionPosition(phase_factor,HessianMatrix,index_i,index_j,f1,f2,dr,1.0);

                      if(Components[TypeMolB].Groups[jg].Rigid)
                      {
                        HessianCenterOfMassOrientationJ(HessianMatrix,index_i,index_j,index_j2,index2,f1,f2,dr);
                        HessianOrientationOrientationJ(HessianMatrix,index_j2,index2,f1,f2,dr);
                      }
                    }
                  }
                }
              }
            }
            ncell++;
          }
    }
  }
}

void ComputeFrameworkCationChargeChargePhonon(VECTOR k,REAL *Energy,REAL* Gradient,COMPLEX_MATRIX HessianMatrix,
                                               REAL_MATRIX3x3 *StrainDerivative,int ComputeGradient,int ComputeHessian)
{
  int J,i,j,jg,ja,fr;
  int typeA,typeB;
  int TypeMolB;
  REAL rr;
  REAL f1,f2;
  VECTOR posA,posB,dr;
  int index_i,index_j,index_j2;
  int index2;
  REAL ChargeA,ChargeB;
  VECTOR comB;
  REAL r,temp1,temp2,temp3;
  int ncell,k1,k2,k3;
  COMPLEX phase_factor;

  f1=f2=0.0;
  index2=0;
  for(fr=0;fr<Framework[CurrentSystem].NumberOfFrameworks;fr++)
  {
    for(i=0;i<Framework[CurrentSystem].NumberOfAtoms[fr];i++)
    {
      index_i=Framework[CurrentSystem].Atoms[fr][i].HessianIndex;
      posA=Framework[CurrentSystem].Atoms[fr][i].Position;
      typeA=Framework[CurrentSystem].Atoms[fr][i].Type;
      ChargeA=Framework[CurrentSystem].Atoms[fr][i].Charge;

      ncell=0;
      for(k1=0;k1<NumberOfReplicaCells[CurrentSystem].x;k1++)
        for(k2=0;k2<NumberOfReplicaCells[CurrentSystem].y;k2++)
          for(k3=0;k3<NumberOfReplicaCells[CurrentSystem].z;k3++)
          {
            for(J=0;J<NumberOfCationMolecules[CurrentSystem];J++)
            {
              TypeMolB=Cations[CurrentSystem][J].Type;
              for(jg=0;jg<Components[TypeMolB].NumberOfGroups;jg++)
              {
                for(ja=0;ja<Components[TypeMolB].Groups[jg].NumberOfGroupAtoms;ja++)
                {
                  j=Components[TypeMolB].Groups[jg].Atoms[ja];

                  if(Components[TypeMolB].Groups[jg].Rigid)
                  {
                    index_j=Cations[CurrentSystem][J].Groups[jg].HessianIndex;
                    index_j2=Cations[CurrentSystem][J].Groups[jg].HessianIndexOrientation;

                    index2=Cations[CurrentSystem][J].Atoms[j].HessianAtomIndex;
                  }
                  else
                  {
                    index_j=Cations[CurrentSystem][J].Atoms[j].HessianIndex;
                    index_j2=-1;
                  }

                  typeB=Cations[CurrentSystem][J].Atoms[j].Type;
                  posB=Cations[CurrentSystem][J].Atoms[j].Position;
                  ChargeB=Cations[CurrentSystem][J].Atoms[j].Charge;

                  posB.x+=ReplicaShift[ncell].x;
                  posB.y+=ReplicaShift[ncell].y;
                  posB.z+=ReplicaShift[ncell].z;

                  dr.x=posA.x-posB.x;
                  dr.y=posA.y-posB.y;
                  dr.z=posA.z-posB.z;
                  dr=ApplyReplicaBoundaryCondition(dr);
                  rr=SQR(dr.x)+SQR(dr.y)+SQR(dr.z);
                  r=sqrt(rr);

                  if(rr<CutOffChargeChargeSquared)
                  {
                    switch(ChargeMethod)
                    {
                      case NONE:
                        f1=f2=0.0;
                        break;
                      case SHIFTED_COULOMB:
                        *Energy+=COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB*(1.0/r-InverseCutOffChargeCharge);
                        f1=-COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB/(rr*r);
                        f2=3.0*COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB/(SQR(rr)*r);
                        break;
                      case TRUNCATED_COULOMB:
                        *Energy+=COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB*(1.0/r);
                        f1=-COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB/(rr*r);
                        f2=3.0*COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB/(SQR(rr)*r);
                        break;
                      case EWALD:
                      default:
                        *Energy+=COULOMBIC_CONVERSION_FACTOR*erfc(Alpha[CurrentSystem]*r)*
                                                ChargeA*ChargeB/r;
                        f1=-COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB*
                            (erfc(Alpha[CurrentSystem]*r)+2.0*Alpha[CurrentSystem]*r*exp(-SQR(Alpha[CurrentSystem])*rr)/sqrt(M_PI))/
                            (r*rr);

                        f2=ChargeA*ChargeB*COULOMBIC_CONVERSION_FACTOR*
                           (((3.0*erfc(Alpha[CurrentSystem]*r)/(r*rr))+
                           (4.0*CUBE(Alpha[CurrentSystem])*exp(-SQR(Alpha[CurrentSystem])*rr)/sqrt(M_PI))+
                           (6.0*Alpha[CurrentSystem]*exp(-SQR(Alpha[CurrentSystem])*rr)/(sqrt(M_PI)*rr)))/(rr));
                        break;
                    }

                    if((index_j<0)&&(index_j2<0)) continue;

                    StrainDerivative->ax+=f1*dr.x*dr.x;
                    StrainDerivative->bx+=f1*dr.x*dr.y;
                    StrainDerivative->cx+=f1*dr.x*dr.z;

                    StrainDerivative->ay+=f1*dr.y*dr.x;
                    StrainDerivative->by+=f1*dr.y*dr.y;
                    StrainDerivative->cy+=f1*dr.y*dr.z;

                    StrainDerivative->az+=f1*dr.z*dr.x;
                    StrainDerivative->bz+=f1*dr.z*dr.y;
                    StrainDerivative->cz+=f1*dr.z*dr.z;

                    if(Components[TypeMolB].Groups[jg].Rigid)
                    {
                      comB=Cations[CurrentSystem][J].Groups[jg].CenterOfMassPosition;
                      comB.x+=ReplicaShift[ncell].x;
                      comB.y+=ReplicaShift[ncell].y;
                      comB.z+=ReplicaShift[ncell].z;

                      temp1=0.5*((posB.y-comB.y)*f1*dr.x+(posB.x-comB.x)*f1*dr.y);
                      temp2=0.5*((posB.z-comB.z)*f1*dr.x+(posB.x-comB.x)*f1*dr.z);
                      temp3=0.5*((posB.z-comB.z)*f1*dr.y+(posB.y-comB.y)*f1*dr.z);
                      StrainDerivative->ax+=(posB.x-comB.x)*f1*dr.x;
                      StrainDerivative->ay+=temp1;
                      StrainDerivative->az+=temp2;
                      StrainDerivative->bx+=temp1;
                      StrainDerivative->by+=(posB.y-comB.y)*f1*dr.y;
                      StrainDerivative->bz+=temp3;
                      StrainDerivative->cx+=temp2;
                      StrainDerivative->cy+=temp3;
                      StrainDerivative->cz+=(posB.z-comB.z)*f1*dr.z;
                    }

                    // add contribution to the first derivatives
                    if(ComputeGradient)
                    {
                      if(Framework[CurrentSystem].FrameworkModel==FLEXIBLE)
                      {
                        if(index_i>=0)
                        {
                          Gradient[index_i]+=f1*dr.x;
                          Gradient[index_i+1]+=f1*dr.y;
                          Gradient[index_i+2]+=f1*dr.z;
                        }
                      }

                      if(index_j>=0)
                      {
                        Gradient[index_j]-=f1*dr.x;
                        Gradient[index_j+1]-=f1*dr.y;
                        Gradient[index_j+2]-=f1*dr.z;
                      }

                      GradientStrain(Gradient,f1,dr);

                      if(Components[TypeMolB].Groups[jg].Rigid)
                      {
                        GradientStrainJ(Gradient,f1,dr,posB,comB);

                        // add contribution to the first derivatives
                        if(index_j2>=0)
                        {
                          Gradient[index_j2]-=f1*(dr.x*DVecX[index2].x+dr.y*DVecX[index2].y+dr.z*DVecX[index2].z);
                          Gradient[index_j2+1]-=f1*(dr.x*DVecY[index2].x+dr.y*DVecY[index2].y+dr.z*DVecY[index2].z);
                          Gradient[index_j2+2]-=f1*(dr.x*DVecZ[index2].x+dr.y*DVecZ[index2].y+dr.z*DVecZ[index2].z);
                        }
                      }
                    }

                    if(ComputeHessian)
                    {
                      HessianAtomicPositionPosition(phase_factor,HessianMatrix,index_i,index_j,f1,f2,dr,1.0);

                      if(Components[TypeMolB].Groups[jg].Rigid)
                      {
                        HessianCenterOfMassOrientationJ(HessianMatrix,index_i,index_j,index_j2,index2,f1,f2,dr);
                        HessianOrientationOrientationJ(HessianMatrix,index_j2,index2,f1,f2,dr);
                      }
                    }
                  }
                }
              }
            }
            ncell++;
          }
    }
  }
}


void ComputeFrameworkIntraVDWPhonon(VECTOR k,REAL *Energy,REAL* Gradient,COMPLEX_MATRIX HessianMatrix,
                                     REAL_MATRIX3x3 *StrainDerivative,int ComputeGradient,int ComputeHessian)
{
  int i,j,typeA,typeB,f1,f2,start;
  REAL energy,DF,DDF;
  REAL rr,r;
  VECTOR posA,posB,dr;
  int index_i,index_j;
  int ncell,k1,k2,k3,indj;
  REAL ReplicaFactor;
  REAL dot_product;
  COMPLEX phase_factor;

/*
  for(f1=0;f1<Framework[CurrentSystem].NumberOfFrameworks;f1++)
  {
    for(i=0;i<Framework[CurrentSystem].NumberOfAtoms[f1];i++)
    {
      typeA=Framework[CurrentSystem].Atoms[f1][i].Type;
      posA=Framework[CurrentSystem].Atoms[f1][i].Position;
      index_i=Framework[CurrentSystem].Atoms[f1][i].HessianIndex;

      ncell=0;
      for(k1=0;k1<NumberOfReplicaCells[CurrentSystem].x;k1++)
        for(k2=0;k2<NumberOfReplicaCells[CurrentSystem].y;k2++)
          for(k3=0;k3<NumberOfReplicaCells[CurrentSystem].z;k3++)
          {
            if(ncell==0) start=i+1;
            else start=0;
            for(j=start;j<Framework[CurrentSystem].NumberOfAtoms[f1];j++)
            {
              indj=j+ncell*Framework[CurrentSystem].NumberOfAtoms[f1];
              if(!BITVAL(Framework[CurrentSystem].ExclusionMatrix[f1][i][indj],0))
              {
                typeB=Framework[CurrentSystem].Atoms[f1][j].Type;
                posB=Framework[CurrentSystem].Atoms[f1][j].Position;
                index_j=Framework[CurrentSystem].Atoms[f1][j].HessianIndex;

                posB.x+=ReplicaShift[ncell].x;
                posB.y+=ReplicaShift[ncell].y;
                posB.z+=ReplicaShift[ncell].z;

                dr.x=posA.x-posB.x;
                dr.y=posA.y-posB.y;
                dr.z=posA.z-posB.z;
                dr=ApplyReplicaBoundaryCondition(dr);
                rr=SQR(dr.x)+SQR(dr.y)+SQR(dr.z);
                r=sqrt(rr);

                if(rr<CutOffVDWSquared)
                {
                  if(ncell==0) ReplicaFactor=1.0;
                  else ReplicaFactor=0.5;

                  PotentialSecondDerivative(typeA,typeB,rr,&energy,&DF,&DDF);

                  // add contribution to the energy
                  *Energy+=ReplicaFactor*energy;

                  if((index_i<0)&&(index_j)<0) continue;

                  if(ComputeGradient)
                  {
                    // add contribution to the first derivatives
                    if(index_i>=0)
                    {
                      Gradient[index_i]+=DF*dr.x;
                      Gradient[index_i+1]+=DF*dr.y;
                      Gradient[index_i+2]+=DF*dr.z;
                    }

                    if(ncell==0)
                    {
                      if(index_j>=0)
                      {
                        Gradient[index_j]-=DF*dr.x;
                        Gradient[index_j+1]-=DF*dr.y;
                        Gradient[index_j+2]-=DF*dr.z;
                      }
                    }

                    GradientStrain(Gradient,ReplicaFactor*DF,dr);
                  }

                  // add contribution to the strain derivative tensor
                  StrainDerivative->ax+=ReplicaFactor*DF*dr.x*dr.x;
                  StrainDerivative->bx+=ReplicaFactor*DF*dr.y*dr.x;
                  StrainDerivative->cx+=ReplicaFactor*DF*dr.z*dr.x;

                  StrainDerivative->ay+=ReplicaFactor*DF*dr.x*dr.y;
                  StrainDerivative->by+=ReplicaFactor*DF*dr.y*dr.y;
                  StrainDerivative->cy+=ReplicaFactor*DF*dr.z*dr.y;

                  StrainDerivative->az+=ReplicaFactor*DF*dr.x*dr.z;
                  StrainDerivative->bz+=ReplicaFactor*DF*dr.y*dr.z;
                  StrainDerivative->cz+=ReplicaFactor*DF*dr.z*dr.z;

                  if(ComputeHessian)
                  {
                    // form e(-k.r)
                    dot_product=k.x*dr.x+k.y*dr.y+k.z*dr.z;
                    phase_factor.re=cos(dot_product);
                    phase_factor.im=-sin(dot_product);

                    // add contribution to the second derivatives (Hessian matrix)
                    HessianAtomicPositionPosition(phase_factor,HessianMatrix,index_i,index_j,DF,DDF,dr,ReplicaFactor);
                  }
                }
              }
            }
            ncell++;
          }
    }
  }
*/
  for(f1=0;f1<Framework[CurrentSystem].NumberOfFrameworks;f1++)
  {
    for(f2=f1;f2<Framework[CurrentSystem].NumberOfFrameworks;f2++)
    {
      for(i=0;i<Framework[CurrentSystem].NumberOfAtoms[f1];i++)
      {
        typeA=Framework[CurrentSystem].Atoms[f1][i].Type;
        posA=Framework[CurrentSystem].Atoms[f1][i].Position;
        index_i=Framework[CurrentSystem].Atoms[f1][i].HessianIndex;

        ncell=0;
        for(k1=0;k1<NumberOfReplicaCells[CurrentSystem].x;k1++)
          for(k2=0;k2<NumberOfReplicaCells[CurrentSystem].y;k2++)
            for(k3=0;k3<NumberOfReplicaCells[CurrentSystem].z;k3++)
            {
              if((f1==f2)&&(ncell==0)) start=i+1;
              else start=0;
              for(j=start;j<Framework[CurrentSystem].NumberOfAtoms[f2];j++)
              {
                indj=j+ncell*Framework[CurrentSystem].NumberOfAtoms[f2];
                if(f1!=f2?TRUE:!BITVAL(Framework[CurrentSystem].ExclusionMatrix[f1][i][indj],0))
                {
                  typeB=Framework[CurrentSystem].Atoms[f2][j].Type;
                  posB=Framework[CurrentSystem].Atoms[f2][j].Position;
                  index_j=Framework[CurrentSystem].Atoms[f2][j].HessianIndex;

                  posB.x+=ReplicaShift[ncell].x;
                  posB.y+=ReplicaShift[ncell].y;
                  posB.z+=ReplicaShift[ncell].z;

                  dr.x=posA.x-posB.x;
                  dr.y=posA.y-posB.y;
                  dr.z=posA.z-posB.z;
                  dr=ApplyReplicaBoundaryCondition(dr);
                  rr=SQR(dr.x)+SQR(dr.y)+SQR(dr.z);
                  r=sqrt(rr);

                  if(rr<CutOffVDWSquared)
                  {
                    if(ncell==0) ReplicaFactor=1.0;
                    else ReplicaFactor=0.5;

                    PotentialSecondDerivative(typeA,typeB,rr,&energy,&DF,&DDF);

                    // add contribution to the energy
                    *Energy+=ReplicaFactor*energy;

                    if((index_i<0)&&(index_j)<0) continue;

                    if(ComputeGradient)
                    {
                      // add contribution to the first derivatives
                      if(index_i>=0)
                      {
                        Gradient[index_i]+=DF*dr.x;
                        Gradient[index_i+1]+=DF*dr.y;
                        Gradient[index_i+2]+=DF*dr.z;
                      }

                      if(ncell==0)
                      {
                        if(index_j>=0)
                        {
                          Gradient[index_j]-=DF*dr.x;
                          Gradient[index_j+1]-=DF*dr.y;
                          Gradient[index_j+2]-=DF*dr.z;
                        }
                      }

                      GradientStrain(Gradient,ReplicaFactor*DF,dr);
                    }

                    // add contribution to the strain derivative tensor
                    StrainDerivative->ax+=ReplicaFactor*DF*dr.x*dr.x;
                    StrainDerivative->bx+=ReplicaFactor*DF*dr.y*dr.x;
                    StrainDerivative->cx+=ReplicaFactor*DF*dr.z*dr.x;

                    StrainDerivative->ay+=ReplicaFactor*DF*dr.x*dr.y;
                    StrainDerivative->by+=ReplicaFactor*DF*dr.y*dr.y;
                    StrainDerivative->cy+=ReplicaFactor*DF*dr.z*dr.y;

                    StrainDerivative->az+=ReplicaFactor*DF*dr.x*dr.z;
                    StrainDerivative->bz+=ReplicaFactor*DF*dr.y*dr.z;
                    StrainDerivative->cz+=ReplicaFactor*DF*dr.z*dr.z;

                    if(ComputeHessian)
                    {
                      dr.x=posA.x-posB.x;
                      dr.y=posA.y-posB.y;
                      dr.z=posA.z-posB.z;
                      dr=ApplyReplicaBoundaryCondition(dr);

                      // form e(-k.r)
                      dot_product=k.x*dr.x+k.y*dr.y+k.z*dr.z;
                      phase_factor.re=cos(dot_product);
                      phase_factor.im=-sin(dot_product);

                      // add contribution to the second derivatives (Hessian matrix)
                      HessianAtomicPositionPosition(phase_factor,HessianMatrix,index_i,index_j,DF,DDF,dr,ReplicaFactor);
                    }
                  }
                }
              }
              ncell++;
            }
      }
    }
  }
}

void ComputeFrameworkIntraChargeChargePhonon(VECTOR k,REAL *Energy,REAL* Gradient,COMPLEX_MATRIX HessianMatrix,
                                              REAL_MATRIX3x3 *StrainDerivative,int ComputeGradient,int ComputeHessian)
{
  int i,j,typeA,typeB,f1,f2,start;
  REAL ChargeA,ChargeB,DF,DDF;
  REAL rr,r;
  VECTOR posA,posB,dr;
  int index_i,index_j;
  int ncell,k1,k2,k3,indj;
  REAL ReplicaFactor;
  COMPLEX phase_factor;
  REAL dot_product;

  for(f1=0;f1<Framework[CurrentSystem].NumberOfFrameworks;f1++)
  {
    for(f2=f1;f2<Framework[CurrentSystem].NumberOfFrameworks;f2++)
    {
      for(i=0;i<Framework[CurrentSystem].NumberOfAtoms[f1];i++)
      {
        typeA=Framework[CurrentSystem].Atoms[f1][i].Type;
        posA=Framework[CurrentSystem].Atoms[f1][i].Position;
        ChargeA=Framework[CurrentSystem].Atoms[f1][i].Charge;
        index_i=Framework[CurrentSystem].Atoms[f1][i].HessianIndex;

        ncell=0;
        for(k1=0;k1<NumberOfReplicaCells[CurrentSystem].x;k1++)
          for(k2=0;k2<NumberOfReplicaCells[CurrentSystem].y;k2++)
            for(k3=0;k3<NumberOfReplicaCells[CurrentSystem].z;k3++)
            {
              if((f1==f2)&&(ncell==0)) start=i+1;
              else start=0;
              for(j=start;j<Framework[CurrentSystem].NumberOfAtoms[f2];j++)
              {
                indj=j+ncell*Framework[CurrentSystem].NumberOfAtoms[f2];
                if(f1!=f2?TRUE:!BITVAL(Framework[CurrentSystem].ExclusionMatrix[f1][i][indj],0))
                {
                  typeB=Framework[CurrentSystem].Atoms[f2][j].Type;
                  posB=Framework[CurrentSystem].Atoms[f2][j].Position;
                  ChargeB=Framework[CurrentSystem].Atoms[f2][j].Charge;
                  index_j=Framework[CurrentSystem].Atoms[f2][j].HessianIndex;

                  posB.x+=ReplicaShift[ncell].x;
                  posB.y+=ReplicaShift[ncell].y;
                  posB.z+=ReplicaShift[ncell].z;

                  dr.x=posA.x-posB.x;
                  dr.y=posA.y-posB.y;
                  dr.z=posA.z-posB.z;
                  dr=ApplyReplicaBoundaryCondition(dr);
                  rr=SQR(dr.x)+SQR(dr.y)+SQR(dr.z);
                  r=sqrt(rr);

                  if(rr<CutOffChargeChargeSquared)
                  {
                    if(ncell==0) ReplicaFactor=1.0;
                    else ReplicaFactor=0.5;

                    switch(ChargeMethod)
                    {
                      case NONE:
                        DF=DDF=0.0;
                        break;
                      case SHIFTED_COULOMB:
                        *Energy+=ReplicaFactor*COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB*(1.0/r-InverseCutOffChargeCharge);
                        DF=-COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB/(rr*r);
                        DDF=3.0*COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB/(SQR(rr)*r);
                        break;
                      case TRUNCATED_COULOMB:
                        *Energy+=ReplicaFactor*COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB*(1.0/r);
                        DF=-COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB/(rr*r);
                        DDF=3.0*COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB/(SQR(rr)*r);
                        break;
                      case EWALD:
                      default:
                        *Energy+=ReplicaFactor*COULOMBIC_CONVERSION_FACTOR*erfc(Alpha[CurrentSystem]*r)*
                                                ChargeA*ChargeB/r;
                        DF=-COULOMBIC_CONVERSION_FACTOR*ChargeA*ChargeB*
                            (erfc(Alpha[CurrentSystem]*r)+2.0*Alpha[CurrentSystem]*r*exp(-SQR(Alpha[CurrentSystem])*rr)/sqrt(M_PI))/
                            (r*rr);

                        DDF=ChargeA*ChargeB*COULOMBIC_CONVERSION_FACTOR*
                            (((3.0*erfc(Alpha[CurrentSystem]*r)/(r*rr))+
                             (4.0*CUBE(Alpha[CurrentSystem])*exp(-SQR(Alpha[CurrentSystem])*rr)/sqrt(M_PI))+
                             (6.0*Alpha[CurrentSystem]*exp(-SQR(Alpha[CurrentSystem])*rr)/(sqrt(M_PI)*rr)))/(rr));
                        break;
                    }

                    if((index_i<0)&&(index_j)<0) continue;

                    if(ComputeGradient)
                    {
                      // add contribution to the first derivatives
                      if(index_i>=0)
                      {
                        Gradient[index_i]+=DF*dr.x;
                        Gradient[index_i+1]+=DF*dr.y;
                        Gradient[index_i+2]+=DF*dr.z;
                      }

                      if(ncell==0)
                      {
                        if(index_j>=0)
                        {
                          Gradient[index_j]-=DF*dr.x;
                          Gradient[index_j+1]-=DF*dr.y;
                          Gradient[index_j+2]-=DF*dr.z;
                        }
                      }

                      GradientStrain(Gradient,ReplicaFactor*DF,dr);
                    }

                    // add contribution to the strain derivative tensor
                    StrainDerivative->ax+=ReplicaFactor*DF*dr.x*dr.x;
                    StrainDerivative->bx+=ReplicaFactor*DF*dr.y*dr.x;
                    StrainDerivative->cx+=ReplicaFactor*DF*dr.z*dr.x;

                    StrainDerivative->ay+=ReplicaFactor*DF*dr.x*dr.y;
                    StrainDerivative->by+=ReplicaFactor*DF*dr.y*dr.y;
                    StrainDerivative->cy+=ReplicaFactor*DF*dr.z*dr.y;

                    StrainDerivative->az+=ReplicaFactor*DF*dr.x*dr.z;
                    StrainDerivative->bz+=ReplicaFactor*DF*dr.y*dr.z;
                    StrainDerivative->cz+=ReplicaFactor*DF*dr.z*dr.z;

                    if(ComputeHessian)
                    {
                      dr.x=posA.x-posB.x;
                      dr.y=posA.y-posB.y;
                      dr.z=posA.z-posB.z;
                      dr=ApplyReplicaBoundaryCondition(dr);

                      // form e(-k.r)
                      dot_product=k.x*dr.x+k.y*dr.y+k.z*dr.z;
                      phase_factor.re=cos(dot_product);
                      phase_factor.im=-sin(dot_product);

                      // add contribution to the second derivatives (Hessian matrix)
                      HessianAtomicPositionPosition(phase_factor,HessianMatrix,index_i,index_j,DF,DDF,dr,ReplicaFactor);
                    }
                  }
                }
              }
              ncell++;
            }
      }
    }
  }
}



void ComputeFrameworkBondPhonon(VECTOR k,REAL *Energy,REAL* Gradient,COMPLEX_MATRIX HessianMatrix,REAL_MATRIX3x3 *StrainDerivative,int ComputeGradient,int ComputeHessian)
{
  int i;   // loop variable
  int f1;  // loop over all frameworks
  REAL r;  // distance
  REAL rr; // distance squared
  REAL temp,temp2; // temporary
  REAL exp_term;   // temporary
  REAL U;  // energy of a specific interaction
  REAL DF; // first derivative
  REAL DDF;  // second derivative
  VECTOR dr; // atoms separation vector
  int A,B;   // atom-indices
  int index_i,index_j; // indices of the Hessian
  REAL *parms;  // pointer to potential parameter
  COMPLEX phase_factor;
  REAL dot_product;

  for(f1=0;f1<Framework[CurrentSystem].NumberOfFrameworks;f1++)
  {
    for(i=0;i<Framework[CurrentSystem].NumberOfBonds[f1];i++)
    {
      A=Framework[CurrentSystem].Bonds[f1][i].A;
      B=Framework[CurrentSystem].Bonds[f1][i].B;

      index_i=Framework[CurrentSystem].Atoms[f1][A].HessianIndex;
      index_j=Framework[CurrentSystem].Atoms[f1][B].HessianIndex;

      dr.x=Framework[CurrentSystem].Atoms[f1][A].Position.x-
           Framework[CurrentSystem].Atoms[f1][B].Position.x;
      dr.y=Framework[CurrentSystem].Atoms[f1][A].Position.y-
           Framework[CurrentSystem].Atoms[f1][B].Position.y;
      dr.z=Framework[CurrentSystem].Atoms[f1][A].Position.z-
           Framework[CurrentSystem].Atoms[f1][B].Position.z;

      // apply boundary condition
      dr=ApplyBoundaryCondition(dr);

      rr=SQR(dr.x)+SQR(dr.y)+SQR(dr.z);
      r=sqrt(rr);

      parms=(REAL*)&Framework[CurrentSystem].BondArguments[f1][i];

      switch(Framework[CurrentSystem].BondType[f1][i])
      {
        case HARMONIC_BOND:
          // 0.5*p0*SQR(r-p1);
          // ===============================================
          // p_0/k_B [K/A^2]   force constant
          // p_1     [A]       reference bond distance
          U=0.5*parms[0]*SQR(r-parms[1]);
          DF=parms[0]*(r-parms[1])/r;
          DDF=(parms[0]*parms[1])/(r*rr);
          //DDF=(parms[0]/r-DF)/rr;
          break;
        case CORE_SHELL_SPRING:
          U=0.5*parms[0]*SQR(r);
          DF=parms[0];
          DDF=0.0;
          break;
        case MORSE_BOND:
          // p_0*[(1.0-{exp(-p_1*(r-p_2))})^2-1.0]
          // ===============================================
          // p_0/k_B [K]       force constant
          // p_1     [A^-1]    parameter
          // p_2     [A]       reference bond distance
          temp=exp(parms[1]*(parms[2]-r));
          U=parms[0]*(SQR(1.0-temp)-1.0);
          DF=2.0*parms[0]*parms[1]*(1.0-temp)*temp/r;
          DDF=2.0*parms[0]*parms[1]*temp*((1.0+2.0*parms[1]*r)*temp-parms[1]*r-1.0)/(r*rr);
          break;
        case LJ_12_6_BOND:
          // A/r_ij^12-B/r_ij^6
          // ===============================================
          // p_0/k_B [K A^12]
          // p_1/k_B [K A^6]
          temp=CUBE(1.0/rr);
          U=parms[0]*SQR(temp)-parms[1]*temp;
          DF=6.0*(parms[1]*temp-2.0*parms[0]*SQR(temp))/rr;
          DDF=24.0*(7.0*parms[0]*SQR(temp)-2.0*parms[1]*temp)/SQR(rr);
          break;
        case LENNARD_JONES_BOND:
          // 4*p_0*((p_1/r)^12-(p_1/r)^6)
          // ===============================================
          // p_0/k_B [K]
          // p_1     [A]
          temp=CUBE(parms[1]/rr);
          U=4.0*parms[0]*(temp*(temp-1.0));
          DF=24.0*parms[0]*(temp*(1.0-2.0*temp))/rr;
          DDF=96.0*parms[0]*(temp*(7.0*temp-2.0))/SQR(rr);
          break;
        case BUCKINGHAM_BOND:
          // p_0*exp(-p_1 r)-p_2/r^6
          // ===============================================
          // p_0/k_B [K]
          // p_1     [A^-1]
          // p_2/k_B [K A^6]
          temp=parms[2]*CUBE(1.0/rr);
          exp_term=parms[0]*exp(-parms[1]*r);
          U=-temp+exp_term;
          DF=(6.0/rr)*temp-parms[1]*exp_term/r;
          DDF=(-48.0*temp/rr+parms[1]*(1.0+parms[1]*r)*exp_term/r)/rr;
          break;
        case RESTRAINED_HARMONIC_BOND:
          // 0.5*p_0*(r-p_1)^2                   |r-p_1|<=p_2
          // 0.5*p_0*p_2^2+p_0*p_2*(|r-p_1|-p_2) |r-p_1|>p_2
          // ===============================================
          // p_0/k_B [K/A^2]
          // p_1     [A]
          // p_2     [A]
          temp=r-parms[1];
          U=0.5*parms[0]*SQR(MIN2(fabs(temp),parms[2]))
                +parms[0]*parms[2]*MAX2(fabs(temp)-parms[2],(REAL)0.0);
          DF=-parms[0]*(SIGN(MIN2(fabs(temp),parms[2]),temp))/r;
          DDF=fabs(temp)<parms[2]?-parms[0]*parms[1]/(r*rr):parms[0]*SIGN(parms[2],temp)/(r*rr);
          break;
        case QUARTIC_BOND:
          // (1/2)*p_0*(r-p_1)^2+(1/3)*p_2*(r-p_1)^3+(1/4)*p_3*(r-p_1)^4
          // ===========================================================
          // p_0/k_B [K/A^2]
          // p_1     [A]
          // p_2/k_B [K/A^3]
          // p_3/k_B [K/A^4]
          temp=r-parms[1];
          temp2=SQR(r-parms[1]);
          U=0.5*parms[0]*temp2+(1.0/3.0)*parms[2]*temp*temp2+0.25*parms[3]*SQR(temp2);
          DF=temp*(parms[0]+parms[2]*temp+parms[3]*temp2)/r;
          DDF=2.0*parms[3]+(parms[2]-3.0*parms[1]*parms[3])/r+(parms[1]*(parms[0]+parms[1]*(parms[1]*parms[3]-parms[2])))/(r*rr);
          break;
        case CFF_QUARTIC_BOND:
          // p_0*(r-p_1)^2+p_2*(r-p_1)^3+p_3*(r-p_1)^4
          // ===============================================
          // p_0/k_B [K/A^2]
          // p_1     [A]
          // p_2/k_B [K/A^3]
          // p_3/k_B [K/A^4]
          temp=r-parms[1];
          temp2=SQR(r-parms[1]);
          U=parms[0]*temp2+parms[2]*temp*temp2+parms[3]*SQR(temp2);
          DF=temp*(2.0*parms[0]+3.0*parms[2]*temp+4.0*parms[3]*temp2)/r;
          DDF=8.0*parms[3]+(3.0*parms[2]-3.0*parms[1]*4.0*parms[3])/r+(parms[1]*(2.0*parms[0]+parms[1]*(parms[1]*4.0*parms[3]-3.0*parms[2])))/(r*rr);
          break;
        case MM3_BOND:
          // p_0*(r-p_1)^2*(1.0-2.55*(r-p_1)+(7.0/12.0)*2.55^2*(r-p_1)^2)
          // =================================================================
          // p_0     [mdyne/A molecule]
          // p_1     [A]
          temp=r-parms[1];
          temp2=SQR(temp);
          U=parms[0]*temp2*(1.0-2.55*temp+(7.0/12.0)*SQR(2.55)*temp2);
          DF=parms[0]*(2.0+2.55*(4.0*2.55*(7.0/12.0)*temp-3.0)*temp)*temp/r;
          DDF=(parms[0]*(SQR(2.55)*4.0*7.0*temp2*(parms[1]+2.0*r)+12.0*(2.0*parms[1]+2.55*3.0*(SQR(parms[1])-SQR(r)))))/(12.0*SQR(r)*r);
          break;
        case RIGID_BOND:
          U=DF=DDF=0.0;
          break;
        case FIXED_BOND:
          U=DF=DDF=0.0;
          break;
        case MEASURE_BOND:
          U=DF=DDF=0.0;
          break;
        default:
          printf("Undefined Bond potential in routine 'CalculateFrameworkBondHessian' ('framework_hessian.c')\n");
          exit(0);
          break;
      }

      // add contribution to the energy
      *Energy+=U;

      if((index_i<0)&&(index_j<0)) continue;

      if(ComputeGradient)
      {
        // add contribution to the first derivatives
        if(index_i>=0)
        {
          Gradient[index_i]+=DF*dr.x;
          Gradient[index_i+1]+=DF*dr.y;
          Gradient[index_i+2]+=DF*dr.z;
        }

        if(index_j>=0)
        {
          Gradient[index_j]-=DF*dr.x;
          Gradient[index_j+1]-=DF*dr.y;
          Gradient[index_j+2]-=DF*dr.z;
        }

        GradientStrain(Gradient,DF,dr);
      }

      // add contribution to the strain derivative tensor
      StrainDerivative->ax+=dr.x*DF*dr.x;
      StrainDerivative->bx+=dr.y*DF*dr.x;
      StrainDerivative->cx+=dr.z*DF*dr.x;

      StrainDerivative->ay+=dr.x*DF*dr.y;
      StrainDerivative->by+=dr.y*DF*dr.y;
      StrainDerivative->cy+=dr.z*DF*dr.y;

      StrainDerivative->az+=dr.x*DF*dr.z;
      StrainDerivative->bz+=dr.y*DF*dr.z;
      StrainDerivative->cz+=dr.z*DF*dr.z;

      if(ComputeHessian)
      {
        // form e(-k.r)
        dot_product=k.x*dr.x+k.y*dr.y+k.z*dr.z;
        phase_factor.re=cos(dot_product);
        phase_factor.im=-sin(dot_product);

        // add contribution to the second derivatives (Hessian matrix)
        HessianAtomicPositionPosition(phase_factor,HessianMatrix,index_i,index_j,DF,DDF,dr,1.0);
      }
    }
  }
}


static inline void GradientStrainBend(REAL *Gradient,REAL_MATRIX3x3 S)
{
  int n;

  n=NumberOfCoordinatesMinimizationVariables;

  switch(Ensemble[CurrentSystem])
  {
    case NPT:
      Gradient[n]+=S.ax+S.by+S.cz;
      break;
    case NPTPR:
    case NPHPR:
      switch(NPTPRCellType[CurrentSystem])
      {
        case ISOTROPIC:
        case ANISOTROPIC:
          Gradient[n]+=S.ax;
          Gradient[n+1]+=S.by;
          Gradient[n+2]+=S.cz;
          break;
        case REGULAR:
        case REGULAR_UPPER_TRIANGLE:
          Gradient[n]+=S.ax;
          Gradient[n+1]+=S.bx;
          Gradient[n+2]+=S.cx;
          Gradient[n+3]+=S.by;
          Gradient[n+4]+=S.cy;
          Gradient[n+5]+=S.cz;
          break;
        case MONOCLINIC:
        case MONOCLINIC_UPPER_TRIANGLE:
          switch(MonoclinicAngleType[CurrentSystem])
          {
            case MONOCLINIC_ALPHA_ANGLE:
              Gradient[n]+=S.ax;
              Gradient[n+1]+=S.by;
              Gradient[n+2]+=S.cy;
              Gradient[n+3]+=S.cz;
              break;
            case MONOCLINIC_BETA_ANGLE:
              Gradient[n]+=S.ax;
              Gradient[n+1]+=S.cx;
              Gradient[n+2]+=S.by;
              Gradient[n+3]+=S.cz;
              break;
            case MONOCLINIC_GAMMA_ANGLE:
              Gradient[n]+=S.ax;
              Gradient[n+1]+=S.bx;
              Gradient[n+2]+=S.by;
              Gradient[n+3]+=S.cz;
              break;
          }
          break;
        default:
          printf("Unknown NPTPRCellType\n");
          exit(0);
          break;
      }
      break;
    case NVT:
    case NVE:
      break;
  }
}



void ComputeFrameworkBendPhonon(VECTOR k,REAL *Energy,REAL* Gradient,COMPLEX_MATRIX HessianMatrix,REAL_MATRIX3x3 *StrainDerivative,int ComputeGradient,int ComputeHessian)
{
  int i,f1,A,B,C;
  REAL *parms,U;
  REAL CosTheta,Theta,SinTheta,temp,temp2;
  REAL rab,rbc,rac;
  POINT posA,posB,posC;
  VECTOR Rab,Rbc,Rac;
  int index_i,index_j,index_k;
  REAL DTDX,DF,DDF;
  REAL_MATRIX3x3 D2I,D2K,D2IK;
  VECTOR dtA,dtB,dtC;
  REAL_MATRIX3x3 S;
  VECTOR vec_u,vec_v;
  REAL u,v,dot_product;
  COMPLEX phase_factor_ab,phase_factor_bc,phase_factor_ac;

  for(f1=0;f1<Framework[CurrentSystem].NumberOfFrameworks;f1++)
  {
    for(i=0;i<Framework[CurrentSystem].NumberOfBends[f1];i++)
    {
      A=Framework[CurrentSystem].Bends[f1][i].A;
      B=Framework[CurrentSystem].Bends[f1][i].B;
      C=Framework[CurrentSystem].Bends[f1][i].C;
      parms=Framework[CurrentSystem].BendArguments[f1][i];

      index_i=Framework[CurrentSystem].Atoms[f1][A].HessianIndex;
      index_j=Framework[CurrentSystem].Atoms[f1][B].HessianIndex;
      index_k=Framework[CurrentSystem].Atoms[f1][C].HessianIndex;

      posA=Framework[CurrentSystem].Atoms[f1][A].Position;
      posB=Framework[CurrentSystem].Atoms[f1][B].Position;
      posC=Framework[CurrentSystem].Atoms[f1][C].Position;

      Rab.x=posA.x-posB.x;
      Rab.y=posA.y-posB.y;
      Rab.z=posA.z-posB.z;
      Rab=ApplyBoundaryCondition(Rab);
      // form e(-k.r)
      dot_product=k.x*Rab.x+k.y*Rab.y+k.z*Rab.z;
      phase_factor_ab.re=cos(dot_product);
      phase_factor_ab.im=-sin(dot_product);
      vec_u=Rab;
      rab=sqrt(SQR(Rab.x)+SQR(Rab.y)+SQR(Rab.z));
      u=rab;
      Rab.x/=rab;
      Rab.y/=rab;
      Rab.z/=rab;

      Rbc.x=posC.x-posB.x;
      Rbc.y=posC.y-posB.y;
      Rbc.z=posC.z-posB.z;
      Rbc=ApplyBoundaryCondition(Rbc);
      // form e(-k.r)
      dot_product=k.x*Rbc.x+k.y*Rbc.y+k.z*Rbc.z;
      phase_factor_bc.re=cos(dot_product);
      phase_factor_bc.im=-sin(dot_product);
      vec_v=Rbc;
      rbc=sqrt(SQR(Rbc.x)+SQR(Rbc.y)+SQR(Rbc.z));
      v=rbc;
      Rbc.x/=rbc;
      Rbc.y/=rbc;
      Rbc.z/=rbc;

      Rac.x=posC.x-posA.x;
      Rac.y=posC.y-posA.y;
      Rac.z=posC.z-posA.z;
      Rac=ApplyBoundaryCondition(Rac);
      // form e(-k.r)
      dot_product=k.x*Rac.x+k.y*Rac.y+k.z*Rac.z;
      phase_factor_ac.re=cos(dot_product);
      phase_factor_ac.im=-sin(dot_product);
      rac=sqrt(SQR(Rac.x)+SQR(Rac.y)+SQR(Rac.z));
      Rac.x/=rac;
      Rac.y/=rac;
      Rac.z/=rac;

      CosTheta=(Rab.x*Rbc.x+Rab.y*Rbc.y+Rab.z*Rbc.z);
      Theta=acos(CosTheta);
      SinTheta=MAX2((REAL)1.0e-8,sqrt(1.0-SQR(CosTheta)));
      DTDX=-1.0/SinTheta;


      switch(Framework[CurrentSystem].BendType[f1][i])
      {
        case HARMONIC_BEND:
          // (1/2)p_0*(theta-p_1)^2
          // ===============================================
          // p_0/k_B [K/rad^2]
          // p_1     [degrees]
          temp=Theta-parms[1];
          U=0.5*parms[0]*SQR(temp);
          DF=parms[0]*temp*DTDX;
          DDF=parms[0]*SQR(DTDX)+parms[0]*temp*CosTheta*CUBE(DTDX);
          break;
        case CORE_SHELL_BEND:
          // (1/2)p_0*(theta-p_1)^2
          // ===============================================
          // p_0/k_B [K/rad^2]
          // p_1     [degrees]
          temp=Theta-parms[1];
          U=0.5*parms[0]*SQR(temp);
          DF=parms[0]*temp*DTDX;
          DDF=parms[0]*SQR(DTDX)+parms[0]*temp*CosTheta*CUBE(DTDX);
          break;
        case QUARTIC_BEND:
          // (1/2)p_0*(theta-p_1)^2+(1/3)*p_2*(theta-p_1)^3+(1/4)*p_2*(theta-p_1)^4
          // ======================================================================
          // p_0/k_B [K/rad^2]
          // p_1     [degrees]
          // p_2/k_B [K/rad^3]
          // p_3/k_B [K/rad^4]
          temp=(Theta-parms[1]);
          temp2=SQR(temp);
          U=0.5*parms[0]*temp2+(1.0/3.0)*parms[2]*temp*temp2+0.25*parms[3]*SQR(temp2);
          DF=(parms[0]*temp+parms[2]*temp2+parms[3]*temp*temp2)*DTDX;
          DDF=parms[0]*SQR(DTDX)+parms[0]*temp*CosTheta*CUBE(DTDX)
              +2.0*parms[2]*temp*SQR(DTDX)+parms[2]*temp2*CosTheta*CUBE(DTDX)
              +3.0*parms[3]*temp2*SQR(DTDX)+parms[3]*temp*temp2*CosTheta*CUBE(DTDX);
          break;
        case CFF_QUARTIC_BEND:
          // p_0*(theta-p_1)^2+p_2*(theta-p_1)^3+p_3*(theta-p_1)^4
          // =====================================================
          // p_0/k_B [K/rad^2]
          // p_1     [degrees]
          // p_2/k_B [K/rad^3]
          // p_3/k_B [K/rad^4]
          temp=(Theta-parms[1]);
          temp2=SQR(temp);
          U=parms[0]*temp2+parms[2]*temp*temp2+parms[3]*SQR(temp2);
          DF=(2.0*parms[0]*temp+3.0*parms[2]*temp2+4.0*parms[3]*temp*temp2)*DTDX;
          DDF=2.0*parms[0]*SQR(DTDX)+2.0*parms[0]*temp*CosTheta*CUBE(DTDX)
              +6.0*parms[2]*temp*SQR(DTDX)+3.0*parms[2]*temp2*CosTheta*CUBE(DTDX)
              +12.0*parms[3]*temp2*SQR(DTDX)+4.0*parms[3]*temp*temp2*CosTheta*CUBE(DTDX);
          break;
        case HARMONIC_COSINE_BEND:
          // (1/2)*p_0*(cos(theta)-cos(p_1))^2
          // ===============================================
          // p_0/k_B [K]
          // p_1     [degrees]
          U=0.5*parms[0]*SQR(CosTheta-parms[1]);
          DF=parms[0]*(CosTheta-parms[1]);
          DDF=parms[0];
          break;
        case COSINE_BEND:
          // p_0*(1+cos(p_1*theta-p_2))
          // ===============================================
          // p_0/k_B [K]
          // p_1     [-]
          // p_2     [degrees]
          temp=parms[1]*Theta-parms[2];
          U=parms[0]*(1.0+cos(temp));
          DF=-(parms[0]*parms[1]*sin(temp))*DTDX;
          DDF=-parms[0]*parms[1]*(parms[1]*cos(temp)+sin(temp)*CosTheta*DTDX)*SQR(DTDX);
          break;
        case MM3_BEND:
        case MM3_IN_PLANE_BEND:
          // p_0*(theta-p_1)^2(1-0.014*(theta-p_1)+5.6e-5*(theta-p_1)^2-7e-7*(theta-p_1)^3+2.2e-8(theta-p_1)^4)
          // =================================================================================================
          // p_0/k_B [mdyne A/rad^2]
          // p_1     [degrees]
          temp=RAD2DEG*(Theta-parms[1]);
          temp2=SQR(temp);
          U=parms[0]*temp2*(1.0-0.014*temp+5.6e-5*temp2-7.0e-7*temp*temp2+2.2e-8*SQR(temp2));
          DF=parms[0]*RAD2DEG*(2.0-(3.0*0.014-(4.0*5.6e-5-(5.0*7.0e-7-6.0*2.2e-8*temp)*temp)*temp)*temp)*temp*DTDX;
          printf("TO BE DONE!\n");
          exit(0);
          break;
        case FIXED_BEND:
          U=DF=DDF=0.0;
          break;
        case MEASURE_BEND:
          U=DF=DDF=0.0;
          break;
        default:
          U=DF=DDF=0.0;
          printf("Undefined Bend potential\n");
          exit(0);
          break;
      }

      // add contribution to the energy
      *Energy+=U;

      if((index_i<0)&&(index_j<0)&&(index_k<0)) continue;

      // Calculate the components of the derivatives.
      dtA.x=(Rbc.x-CosTheta*Rab.x)/rab;
      dtA.y=(Rbc.y-CosTheta*Rab.y)/rab;
      dtA.z=(Rbc.z-CosTheta*Rab.z)/rab;

      dtC.x=(Rab.x-CosTheta*Rbc.x)/rbc;
      dtC.y=(Rab.y-CosTheta*Rbc.y)/rbc;
      dtC.z=(Rab.z-CosTheta*Rbc.z)/rbc;

      dtB.x=-(dtA.x+dtC.x);
      dtB.y=-(dtA.y+dtC.y);
      dtB.z=-(dtA.z+dtC.z);

      S.ax=rab*Rab.x*DF*dtA.x+rbc*Rbc.x*DF*dtC.x;
      S.bx=rab*Rab.y*DF*dtA.x+rbc*Rbc.y*DF*dtC.x;
      S.cx=rab*Rab.z*DF*dtA.x+rbc*Rbc.z*DF*dtC.x;

      S.ay=rab*Rab.x*DF*dtA.y+rbc*Rbc.x*DF*dtC.y;
      S.by=rab*Rab.y*DF*dtA.y+rbc*Rbc.y*DF*dtC.y;
      S.cy=rab*Rab.z*DF*dtA.y+rbc*Rbc.z*DF*dtC.y;

      S.az=rab*Rab.x*DF*dtA.z+rbc*Rbc.x*DF*dtC.z;
      S.bz=rab*Rab.y*DF*dtA.z+rbc*Rbc.y*DF*dtC.z;
      S.cz=rab*Rab.z*DF*dtA.z+rbc*Rbc.z*DF*dtC.z;

      StrainDerivative->ax+=S.ax;
      StrainDerivative->bx+=S.bx;
      StrainDerivative->cx+=S.cx;

      StrainDerivative->ay+=S.ay;
      StrainDerivative->by+=S.by;
      StrainDerivative->cy+=S.cy;

      StrainDerivative->az+=S.az;
      StrainDerivative->bz+=S.bz;
      StrainDerivative->cz+=S.cz;

      // add contribution to the first derivatives
      if(ComputeGradient)
      {
        if(index_i>=0)
        {
          Gradient[index_i]+=DF*dtA.x;
          Gradient[index_i+1]+=DF*dtA.y;
          Gradient[index_i+2]+=DF*dtA.z;
        }

        if(index_j>=0)
        {
          Gradient[index_j]+=DF*dtB.x;
          Gradient[index_j+1]+=DF*dtB.y;
          Gradient[index_j+2]+=DF*dtB.z;
        }

        if(index_k>=0)
        {
          Gradient[index_k]+=DF*dtC.x;
          Gradient[index_k+1]+=DF*dtC.y;
          Gradient[index_k+2]+=DF*dtC.z;
        }

        GradientStrainBend(Gradient,S);
      }

      if(ComputeHessian)
      {
        // Calculate some diagonal Hessian terms for A
        D2I.ax=-DF*(2.0*dtA.x*Rab.x+CosTheta*(1.0-Rab.x*Rab.x)/rab)/rab;
        D2I.by=-DF*(2.0*dtA.y*Rab.y+CosTheta*(1.0-Rab.y*Rab.y)/rab)/rab;
        D2I.cz=-DF*(2.0*dtA.z*Rab.z+CosTheta*(1.0-Rab.z*Rab.z)/rab)/rab;
        D2I.ay=DF*(CosTheta*Rab.x*Rab.y/rab-dtA.x*Rab.y-dtA.y*Rab.x)/rab;
        D2I.az=DF*(CosTheta*Rab.x*Rab.z/rab-dtA.x*Rab.z-dtA.z*Rab.x)/rab;
        D2I.bz=DF*(CosTheta*Rab.y*Rab.z/rab-dtA.y*Rab.z-dtA.z*Rab.y)/rab;

        // Calculate some diagonal Hessian terms for C
        D2K.ax=-DF*(2.0*dtC.x*Rbc.x+CosTheta*(1.0-Rbc.x*Rbc.x)/rbc)/rbc;
        D2K.by=-DF*(2.0*dtC.y*Rbc.y+CosTheta*(1.0-Rbc.y*Rbc.y)/rbc)/rbc;
        D2K.cz=-DF*(2.0*dtC.z*Rbc.z+CosTheta*(1.0-Rbc.z*Rbc.z)/rbc)/rbc;
        D2K.ay=DF*(CosTheta*Rbc.x*Rbc.y/rbc-dtC.x*Rbc.y-dtC.y*Rbc.x)/rbc;
        D2K.az=DF*(CosTheta*Rbc.x*Rbc.z/rbc-dtC.x*Rbc.z-dtC.z*Rbc.x)/rbc;
        D2K.bz=DF*(CosTheta*Rbc.y*Rbc.z/rbc-dtC.y*Rbc.z-dtC.z*Rbc.y)/rbc;

        // Calculate the AC off-diagonal terms.
        D2IK.ax=DF*(CosTheta*Rab.x*Rbc.x-Rab.x*Rab.x-Rbc.x*Rbc.x+1.0)/(rab*rbc);
        D2IK.ay=DF*(CosTheta*Rab.x*Rbc.y-Rab.x*Rab.y-Rbc.x*Rbc.y)/(rab*rbc);
        D2IK.az=DF*(CosTheta*Rab.x*Rbc.z-Rab.x*Rab.z-Rbc.x*Rbc.z)/(rab*rbc);
        D2IK.bx=DF*(CosTheta*Rab.y*Rbc.x-Rab.y*Rab.x-Rbc.y*Rbc.x)/(rab*rbc);
        D2IK.by=DF*(CosTheta*Rab.y*Rbc.y-Rab.y*Rab.y-Rbc.y*Rbc.y+1.0)/(rab*rbc);
        D2IK.bz=DF*(CosTheta*Rab.y*Rbc.z-Rab.y*Rab.z-Rbc.y*Rbc.z)/(rab*rbc);
        D2IK.cx=DF*(CosTheta*Rab.z*Rbc.x-Rab.z*Rab.x-Rbc.z*Rbc.x)/(rab*rbc);
        D2IK.cy=DF*(CosTheta*Rab.z*Rbc.y-Rab.z*Rab.y-Rbc.z*Rbc.y)/(rab*rbc);
        D2IK.cz=DF*(CosTheta*Rab.z*Rbc.z-Rab.z*Rab.z-Rbc.z*Rbc.z+1.0)/(rab*rbc);

        // Calculate AA-block of the Hessian
        if(index_i>=0)
        {
          HessianMatrix.element[index_i][index_i].re+=DDF*dtA.x*dtA.x+D2I.ax;
          HessianMatrix.element[index_i][index_i+1].re+=DDF*dtA.x*dtA.y+D2I.ay;
          HessianMatrix.element[index_i+1][index_i+1].re+=DDF*dtA.y*dtA.y+D2I.by;
          HessianMatrix.element[index_i][index_i+2].re+=DDF*dtA.x*dtA.z+D2I.az;
          HessianMatrix.element[index_i+1][index_i+2].re+=DDF*dtA.y*dtA.z+D2I.bz;
          HessianMatrix.element[index_i+2][index_i+2].re+=DDF*dtA.z*dtA.z+D2I.cz;
        }

        // Calculate BB-block of the Hessian
        if(index_j>=0)
        {
          HessianMatrix.element[index_j][index_j].re+=DDF*dtB.x*dtB.x+(D2I.ax+D2K.ax+2.0*D2IK.ax);
          HessianMatrix.element[index_j][index_j+1].re+=DDF*dtB.x*dtB.y+(D2I.ay+D2K.ay+D2IK.ay+D2IK.bx);
          HessianMatrix.element[index_j+1][index_j+1].re+=DDF*dtB.y*dtB.y+(D2I.by+D2K.by+2.0*D2IK.by);
          HessianMatrix.element[index_j][index_j+2].re+=DDF*dtB.x*dtB.z+(D2I.az+D2K.az+D2IK.az+D2IK.cx);
          HessianMatrix.element[index_j+1][index_j+2].re+=DDF*dtB.y*dtB.z+(D2I.bz+D2K.bz+D2IK.bz+D2IK.cy);
          HessianMatrix.element[index_j+2][index_j+2].re+=DDF*dtB.z*dtB.z+(D2I.cz+D2K.cz+2.0*D2IK.cz);
        }

        // Calculate the CC-block of the Hessian
        if(index_k>=0)
        {
          HessianMatrix.element[index_k][index_k].re+=DDF*dtC.x*dtC.x+D2K.ax;
          HessianMatrix.element[index_k][index_k+1].re+=DDF*dtC.x*dtC.y+D2K.ay;
          HessianMatrix.element[index_k+1][index_k+1].re+=DDF*dtC.y*dtC.y+D2K.by;
          HessianMatrix.element[index_k][index_k+2].re+=DDF*dtC.x*dtC.z+D2K.az;
          HessianMatrix.element[index_k+1][index_k+2].re+=DDF*dtC.y*dtC.z+D2K.bz;
          HessianMatrix.element[index_k+2][index_k+2].re+=DDF*dtC.z*dtC.z+D2K.cz;
        }

        // Calculate the AB-block of the Hessian
        if((index_i>=0)&&(index_j>=0))
        {
          if(index_i<index_j)
          {
            HessianMatrix.element[index_i][index_j].re+=phase_factor_ab.re*(DDF*dtA.x*dtB.x-D2I.ax-D2IK.ax);
            HessianMatrix.element[index_i][index_j].im+=phase_factor_ab.im*(DDF*dtA.x*dtB.x-D2I.ax-D2IK.ax);
            HessianMatrix.element[index_i][index_j+1].re+=phase_factor_ab.re*(DDF*dtA.x*dtB.y-D2I.ay-D2IK.ay);
            HessianMatrix.element[index_i][index_j+1].im+=phase_factor_ab.im*(DDF*dtA.x*dtB.y-D2I.ay-D2IK.ay);
            HessianMatrix.element[index_i][index_j+2].re+=phase_factor_ab.re*(DDF*dtA.x*dtB.z-D2I.az-D2IK.az);
            HessianMatrix.element[index_i][index_j+2].im+=phase_factor_ab.im*(DDF*dtA.x*dtB.z-D2I.az-D2IK.az);
            HessianMatrix.element[index_i+1][index_j].re+=phase_factor_ab.re*(DDF*dtA.y*dtB.x-D2I.ay-D2IK.bx);
            HessianMatrix.element[index_i+1][index_j].im+=phase_factor_ab.im*(DDF*dtA.y*dtB.x-D2I.ay-D2IK.bx);
            HessianMatrix.element[index_i+1][index_j+1].re+=phase_factor_ab.re*(DDF*dtA.y*dtB.y-D2I.by-D2IK.by);
            HessianMatrix.element[index_i+1][index_j+1].im+=phase_factor_ab.im*(DDF*dtA.y*dtB.y-D2I.by-D2IK.by);
            HessianMatrix.element[index_i+1][index_j+2].re+=phase_factor_ab.re*(DDF*dtA.y*dtB.z-D2I.bz-D2IK.bz);
            HessianMatrix.element[index_i+1][index_j+2].im+=phase_factor_ab.im*(DDF*dtA.y*dtB.z-D2I.bz-D2IK.bz);
            HessianMatrix.element[index_i+2][index_j].re+=phase_factor_ab.re*(DDF*dtA.z*dtB.x-D2I.az-D2IK.cx);
            HessianMatrix.element[index_i+2][index_j].im+=phase_factor_ab.im*(DDF*dtA.z*dtB.x-D2I.az-D2IK.cx);
            HessianMatrix.element[index_i+2][index_j+1].re+=phase_factor_ab.re*(DDF*dtA.z*dtB.y-D2I.bz-D2IK.cy);
            HessianMatrix.element[index_i+2][index_j+1].im+=phase_factor_ab.im*(DDF*dtA.z*dtB.y-D2I.bz-D2IK.cy);
            HessianMatrix.element[index_i+2][index_j+2].re+=phase_factor_ab.re*(DDF*dtA.z*dtB.z-D2I.cz-D2IK.cz);
            HessianMatrix.element[index_i+2][index_j+2].im+=phase_factor_ab.im*(DDF*dtA.z*dtB.z-D2I.cz-D2IK.cz);
          }
          else
          {
            HessianMatrix.element[index_j][index_i].re+=phase_factor_ab.re*(DDF*dtA.x*dtB.x-D2I.ax-D2IK.ax);
            HessianMatrix.element[index_j][index_i].im+=phase_factor_ab.im*(DDF*dtA.x*dtB.x-D2I.ax-D2IK.ax);
            HessianMatrix.element[index_j][index_i+1].re+=phase_factor_ab.re*(DDF*dtA.y*dtB.x-D2I.ay-D2IK.bx);
            HessianMatrix.element[index_j][index_i+1].im+=phase_factor_ab.im*(DDF*dtA.y*dtB.x-D2I.ay-D2IK.bx);
            HessianMatrix.element[index_j][index_i+2].re+=phase_factor_ab.re*(DDF*dtA.z*dtB.x-D2I.az-D2IK.cx);
            HessianMatrix.element[index_j][index_i+2].im+=phase_factor_ab.im*(DDF*dtA.z*dtB.x-D2I.az-D2IK.cx);
            HessianMatrix.element[index_j+1][index_i].re+=phase_factor_ab.re*(DDF*dtA.x*dtB.y-D2I.ay-D2IK.ay);
            HessianMatrix.element[index_j+1][index_i].im+=phase_factor_ab.im*(DDF*dtA.x*dtB.y-D2I.ay-D2IK.ay);
            HessianMatrix.element[index_j+1][index_i+1].re+=phase_factor_ab.re*(DDF*dtA.y*dtB.y-D2I.by-D2IK.by);
            HessianMatrix.element[index_j+1][index_i+1].im+=phase_factor_ab.im*(DDF*dtA.y*dtB.y-D2I.by-D2IK.by);
            HessianMatrix.element[index_j+1][index_i+2].re+=phase_factor_ab.re*(DDF*dtA.z*dtB.y-D2I.bz-D2IK.cy);
            HessianMatrix.element[index_j+1][index_i+2].im+=phase_factor_ab.im*(DDF*dtA.z*dtB.y-D2I.bz-D2IK.cy);
            HessianMatrix.element[index_j+2][index_i].re+=phase_factor_ab.re*(DDF*dtA.x*dtB.z-D2I.az-D2IK.az);
            HessianMatrix.element[index_j+2][index_i].im+=phase_factor_ab.im*(DDF*dtA.x*dtB.z-D2I.az-D2IK.az);
            HessianMatrix.element[index_j+2][index_i+1].re+=phase_factor_ab.re*(DDF*dtA.y*dtB.z-D2I.bz-D2IK.bz);
            HessianMatrix.element[index_j+2][index_i+1].im+=phase_factor_ab.im*(DDF*dtA.y*dtB.z-D2I.bz-D2IK.bz);
            HessianMatrix.element[index_j+2][index_i+2].re+=phase_factor_ab.re*(DDF*dtA.z*dtB.z-D2I.cz-D2IK.cz);
            HessianMatrix.element[index_j+2][index_i+2].im+=phase_factor_ab.im*(DDF*dtA.z*dtB.z-D2I.cz-D2IK.cz);
          }
        }

        // Calculate the AC-block of the Hessian
        if((index_i>=0)&&(index_k>=0))
        {
          if(index_i<index_k)
          {
            HessianMatrix.element[index_i][index_k].re+=phase_factor_ac.re*(DDF*dtA.x*dtC.x+D2IK.ax);
            HessianMatrix.element[index_i][index_k].im+=phase_factor_ac.im*(DDF*dtA.x*dtC.x+D2IK.ax);
            HessianMatrix.element[index_i][index_k+1].re+=phase_factor_ac.re*(DDF*dtA.x*dtC.y+D2IK.ay);
            HessianMatrix.element[index_i][index_k+1].im+=phase_factor_ac.im*(DDF*dtA.x*dtC.y+D2IK.ay);
            HessianMatrix.element[index_i][index_k+2].re+=phase_factor_ac.re*(DDF*dtA.x*dtC.z+D2IK.az);
            HessianMatrix.element[index_i][index_k+2].im+=phase_factor_ac.im*(DDF*dtA.x*dtC.z+D2IK.az);
            HessianMatrix.element[index_i+1][index_k].re+=phase_factor_ac.re*(DDF*dtA.y*dtC.x+D2IK.bx);
            HessianMatrix.element[index_i+1][index_k].im+=phase_factor_ac.im*(DDF*dtA.y*dtC.x+D2IK.bx);
            HessianMatrix.element[index_i+1][index_k+1].re+=phase_factor_ac.re*(DDF*dtA.y*dtC.y+D2IK.by);
            HessianMatrix.element[index_i+1][index_k+1].im+=phase_factor_ac.im*(DDF*dtA.y*dtC.y+D2IK.by);
            HessianMatrix.element[index_i+1][index_k+2].re+=phase_factor_ac.re*(DDF*dtA.y*dtC.z+D2IK.bz);
            HessianMatrix.element[index_i+1][index_k+2].im+=phase_factor_ac.im*(DDF*dtA.y*dtC.z+D2IK.bz);
            HessianMatrix.element[index_i+2][index_k].re+=phase_factor_ac.re*(DDF*dtA.z*dtC.x+D2IK.cx);
            HessianMatrix.element[index_i+2][index_k].im+=phase_factor_ac.im*(DDF*dtA.z*dtC.x+D2IK.cx);
            HessianMatrix.element[index_i+2][index_k+1].re+=phase_factor_ac.re*(DDF*dtA.z*dtC.y+D2IK.cy);
            HessianMatrix.element[index_i+2][index_k+1].im+=phase_factor_ac.im*(DDF*dtA.z*dtC.y+D2IK.cy);
            HessianMatrix.element[index_i+2][index_k+2].re+=phase_factor_ac.re*(DDF*dtA.z*dtC.z+D2IK.cz);
            HessianMatrix.element[index_i+2][index_k+2].im+=phase_factor_ac.im*(DDF*dtA.z*dtC.z+D2IK.cz);
          }
          else 
          {
            HessianMatrix.element[index_k][index_i].re+=phase_factor_ac.re*(DDF*dtA.x*dtC.x+D2IK.ax);
            HessianMatrix.element[index_k][index_i].im+=phase_factor_ac.im*(DDF*dtA.x*dtC.x+D2IK.ax);
            HessianMatrix.element[index_k][index_i+1].re+=phase_factor_ac.re*(DDF*dtA.y*dtC.x+D2IK.bx);
            HessianMatrix.element[index_k][index_i+1].im+=phase_factor_ac.im*(DDF*dtA.y*dtC.x+D2IK.bx);
            HessianMatrix.element[index_k][index_i+2].re+=phase_factor_ac.re*(DDF*dtA.z*dtC.x+D2IK.cx);
            HessianMatrix.element[index_k][index_i+2].im+=phase_factor_ac.im*(DDF*dtA.z*dtC.x+D2IK.cx);
            HessianMatrix.element[index_k+1][index_i].re+=phase_factor_ac.re*(DDF*dtA.x*dtC.y+D2IK.ay);
            HessianMatrix.element[index_k+1][index_i].im+=phase_factor_ac.im*(DDF*dtA.x*dtC.y+D2IK.ay);
            HessianMatrix.element[index_k+1][index_i+1].re+=phase_factor_ac.re*(DDF*dtA.y*dtC.y+D2IK.by);
            HessianMatrix.element[index_k+1][index_i+1].im+=phase_factor_ac.im*(DDF*dtA.y*dtC.y+D2IK.by);
            HessianMatrix.element[index_k+1][index_i+2].re+=phase_factor_ac.re*(DDF*dtA.z*dtC.y+D2IK.cy);
            HessianMatrix.element[index_k+1][index_i+2].im+=phase_factor_ac.im*(DDF*dtA.z*dtC.y+D2IK.cy);
            HessianMatrix.element[index_k+2][index_i].re+=phase_factor_ac.re*(DDF*dtA.x*dtC.z+D2IK.az);
            HessianMatrix.element[index_k+2][index_i].im+=phase_factor_ac.im*(DDF*dtA.x*dtC.z+D2IK.az);
            HessianMatrix.element[index_k+2][index_i+1].re+=phase_factor_ac.re*(DDF*dtA.y*dtC.z+D2IK.bz);
            HessianMatrix.element[index_k+2][index_i+1].im+=phase_factor_ac.im*(DDF*dtA.y*dtC.z+D2IK.bz);
            HessianMatrix.element[index_k+2][index_i+2].re+=phase_factor_ac.re*(DDF*dtA.z*dtC.z+D2IK.cz);
            HessianMatrix.element[index_k+2][index_i+2].im+=phase_factor_ac.im*(DDF*dtA.z*dtC.z+D2IK.cz);
          }
        }

        // Calculate the BC-block of the Hessian
        if((index_j>=0)&&(index_k>=0))
        {
          if(index_k<index_j)
          {
            HessianMatrix.element[index_k][index_j].re+=phase_factor_bc.re*(DDF*dtC.x*dtB.x-D2K.ax-D2IK.ax);
            HessianMatrix.element[index_k][index_j].im+=phase_factor_bc.im*(DDF*dtC.x*dtB.x-D2K.ax-D2IK.ax);
            HessianMatrix.element[index_k][index_j+1].re+=phase_factor_bc.re*(DDF*dtC.x*dtB.y-D2K.ay-D2IK.bx);
            HessianMatrix.element[index_k][index_j+1].im+=phase_factor_bc.im*(DDF*dtC.x*dtB.y-D2K.ay-D2IK.bx);
            HessianMatrix.element[index_k][index_j+2].re+=phase_factor_bc.re*(DDF*dtC.x*dtB.z-D2K.az-D2IK.cx);
            HessianMatrix.element[index_k][index_j+2].im+=phase_factor_bc.im*(DDF*dtC.x*dtB.z-D2K.az-D2IK.cx);
            HessianMatrix.element[index_k+1][index_j].re+=phase_factor_bc.re*(DDF*dtC.y*dtB.x-D2K.ay-D2IK.ay);
            HessianMatrix.element[index_k+1][index_j].im+=phase_factor_bc.im*(DDF*dtC.y*dtB.x-D2K.ay-D2IK.ay);
            HessianMatrix.element[index_k+1][index_j+1].re+=phase_factor_bc.re*(DDF*dtC.y*dtB.y-D2K.by-D2IK.by);
            HessianMatrix.element[index_k+1][index_j+1].im+=phase_factor_bc.im*(DDF*dtC.y*dtB.y-D2K.by-D2IK.by);
            HessianMatrix.element[index_k+1][index_j+2].re+=phase_factor_bc.re*(DDF*dtC.y*dtB.z-D2K.bz-D2IK.cy);
            HessianMatrix.element[index_k+1][index_j+2].im+=phase_factor_bc.im*(DDF*dtC.y*dtB.z-D2K.bz-D2IK.cy);
            HessianMatrix.element[index_k+2][index_j].re+=phase_factor_bc.re*(DDF*dtC.z*dtB.x-D2K.az-D2IK.az);
            HessianMatrix.element[index_k+2][index_j].im+=phase_factor_bc.im*(DDF*dtC.z*dtB.x-D2K.az-D2IK.az);
            HessianMatrix.element[index_k+2][index_j+1].re+=phase_factor_bc.re*(DDF*dtC.z*dtB.y-D2K.bz-D2IK.bz);
            HessianMatrix.element[index_k+2][index_j+1].im+=phase_factor_bc.im*(DDF*dtC.z*dtB.y-D2K.bz-D2IK.bz);
            HessianMatrix.element[index_k+2][index_j+2].re+=phase_factor_bc.re*(DDF*dtC.z*dtB.z-D2K.cz-D2IK.cz);
            HessianMatrix.element[index_k+2][index_j+2].im+=phase_factor_bc.im*(DDF*dtC.z*dtB.z-D2K.cz-D2IK.cz);
          }
          else 
          {
            HessianMatrix.element[index_j][index_k].re+=phase_factor_bc.re*(DDF*dtC.x*dtB.x-D2K.ax-D2IK.ax);
            HessianMatrix.element[index_j][index_k].im+=phase_factor_bc.im*(DDF*dtC.x*dtB.x-D2K.ax-D2IK.ax);
            HessianMatrix.element[index_j][index_k+1].re+=phase_factor_bc.re*(DDF*dtC.y*dtB.x-D2K.ay-D2IK.ay);
            HessianMatrix.element[index_j][index_k+1].im+=phase_factor_bc.im*(DDF*dtC.y*dtB.x-D2K.ay-D2IK.ay);
            HessianMatrix.element[index_j][index_k+2].re+=phase_factor_bc.re*(DDF*dtC.z*dtB.x-D2K.az-D2IK.az);
            HessianMatrix.element[index_j][index_k+2].im+=phase_factor_bc.im*(DDF*dtC.z*dtB.x-D2K.az-D2IK.az);
            HessianMatrix.element[index_j+1][index_k].re+=phase_factor_bc.re*(DDF*dtC.x*dtB.y-D2K.ay-D2IK.bx);
            HessianMatrix.element[index_j+1][index_k].im+=phase_factor_bc.im*(DDF*dtC.x*dtB.y-D2K.ay-D2IK.bx);
            HessianMatrix.element[index_j+1][index_k+1].re+=phase_factor_bc.re*(DDF*dtC.y*dtB.y-D2K.by-D2IK.by);
            HessianMatrix.element[index_j+1][index_k+1].im+=phase_factor_bc.im*(DDF*dtC.y*dtB.y-D2K.by-D2IK.by);
            HessianMatrix.element[index_j+1][index_k+2].re+=phase_factor_bc.re*(DDF*dtC.z*dtB.y-D2K.bz-D2IK.bz);
            HessianMatrix.element[index_j+1][index_k+2].im+=phase_factor_bc.im*(DDF*dtC.z*dtB.y-D2K.bz-D2IK.bz);
            HessianMatrix.element[index_j+2][index_k].re+=phase_factor_bc.re*(DDF*dtC.x*dtB.z-D2K.az-D2IK.cx);
            HessianMatrix.element[index_j+2][index_k].im+=phase_factor_bc.im*(DDF*dtC.x*dtB.z-D2K.az-D2IK.cx);
            HessianMatrix.element[index_j+2][index_k+1].re+=phase_factor_bc.re*(DDF*dtC.y*dtB.z-D2K.bz-D2IK.cy);
            HessianMatrix.element[index_j+2][index_k+1].im+=phase_factor_bc.im*(DDF*dtC.y*dtB.z-D2K.bz-D2IK.cy);
            HessianMatrix.element[index_j+2][index_k+2].re+=phase_factor_bc.re*(DDF*dtC.z*dtB.z-D2K.cz-D2IK.cz);
            HessianMatrix.element[index_j+2][index_k+2].im+=phase_factor_bc.im*(DDF*dtC.z*dtB.z-D2K.cz-D2IK.cz);
          }
        }
      }
    }
  }

}

static inline void GradientStrainTorsion(REAL *Gradient,REAL_MATRIX3x3 S)
{
  int n;

  n=NumberOfCoordinatesMinimizationVariables;
  switch(Ensemble[CurrentSystem])
  {
    case NPT:
      Gradient[n]+=S.ax+S.by+S.cz;
      break;
    case NPTPR:
    case NPHPR:
      switch(NPTPRCellType[CurrentSystem])
      {
        case ISOTROPIC:
        case ANISOTROPIC:
          Gradient[n]+=S.ax;
          Gradient[n+1]+=S.by;
          Gradient[n+2]+=S.cz;
          break;
        case REGULAR:
        case REGULAR_UPPER_TRIANGLE:
          Gradient[n]+=S.ax;
          Gradient[n+1]+=S.bx;
          Gradient[n+2]+=S.cx;
          Gradient[n+3]+=S.by;
          Gradient[n+4]+=S.cy;
          Gradient[n+5]+=S.cz;
          break;
        case MONOCLINIC:
        case MONOCLINIC_UPPER_TRIANGLE:
          switch(MonoclinicAngleType[CurrentSystem])
          {
            case MONOCLINIC_ALPHA_ANGLE:
              Gradient[n]+=S.ax;
              Gradient[n+1]+=S.by;
              Gradient[n+2]+=S.cy;
              Gradient[n+3]+=S.cz;
              break;
            case MONOCLINIC_BETA_ANGLE:
              Gradient[n]+=S.ax;
              Gradient[n+1]+=S.cx;
              Gradient[n+2]+=S.by;
              Gradient[n+3]+=S.cz;
              break;
            case MONOCLINIC_GAMMA_ANGLE:
              Gradient[n]+=S.ax;
              Gradient[n+1]+=S.bx;
              Gradient[n+2]+=S.by;
              Gradient[n+3]+=S.cz;
              break;
          }
          break;
        default:
          printf("Unknown NPTPRCellType\n");
          exit(0);
          break;
      }
      break;
    case NVT:
    case NVE:
      break;
  }
}



void ComputeFrameworkTorsionPhonon(VECTOR k,REAL *Energy,REAL* Gradient,COMPLEX_MATRIX HessianMatrix,REAL_MATRIX3x3 *StrainDerivative,int ComputeGradient,int ComputeHessian)
{
  int i,A,B,C,D,f1;
  POINT posA,posB,posC,posD;
  REAL d,e,rbc;
  VECTOR Dab,Dcb,Ddc,dr,ds;
  REAL dot_ab,dot_cd,r,s,sign;
  REAL U,CosPhi,Phi,CosPhi2,SinPhi;
  VECTOR dtA,dtB,dtC,dtD,Pb,Pc;
  REAL *parms;
  int index_i,index_j,index_k,index_l;
  REAL_MATRIX3x3 D2I,D2L,D2IL,D2IJ,D2IK,D2JL;
  REAL_MATRIX3x3 D2KL,D2J,D2K,D2JK,S;
  VECTOR DIL,DDJ,DDK,DEJ,DEK;
  REAL DF,DDF;
  VECTOR vec_u,vec_v,vec_w;
  REAL u,w,v;
  VECTOR fa,fb,fc,fd;

  for(f1=0;f1<Framework[CurrentSystem].NumberOfFrameworks;f1++)
  {
    for(i=0;i<Framework[CurrentSystem].NumberOfTorsions[f1];i++)
    {
      A=Framework[CurrentSystem].Torsions[f1][i].A;
      B=Framework[CurrentSystem].Torsions[f1][i].B;
      C=Framework[CurrentSystem].Torsions[f1][i].C;
      D=Framework[CurrentSystem].Torsions[f1][i].D;
      parms=Framework[CurrentSystem].TorsionArguments[f1][i];

      posA=Framework[CurrentSystem].Atoms[f1][A].Position;
      posB=Framework[CurrentSystem].Atoms[f1][B].Position;
      posC=Framework[CurrentSystem].Atoms[f1][C].Position;
      posD=Framework[CurrentSystem].Atoms[f1][D].Position;

      index_i=Framework[CurrentSystem].Atoms[f1][A].HessianIndex;
      index_j=Framework[CurrentSystem].Atoms[f1][B].HessianIndex;
      index_k=Framework[CurrentSystem].Atoms[f1][C].HessianIndex;
      index_l=Framework[CurrentSystem].Atoms[f1][D].HessianIndex;

      vec_u.x=posB.x-posC.x;
      vec_u.y=posB.y-posC.y;
      vec_u.z=posB.z-posC.z;
      vec_u=ApplyBoundaryCondition(vec_u);
      u=sqrt(SQR(vec_u.x)+SQR(vec_u.y)+SQR(vec_u.z));

      vec_v.x=posD.x-posC.x;
      vec_v.y=posD.y-posC.y;
      vec_v.z=posD.z-posC.z;
      vec_v=ApplyBoundaryCondition(vec_v);
      v=sqrt(SQR(vec_v.x)+SQR(vec_v.y)+SQR(vec_v.z));

      vec_w.x=posA.x-posB.x;
      vec_w.y=posA.y-posB.y;
      vec_w.z=posA.z-posB.z;
      vec_w=ApplyBoundaryCondition(vec_w);
      w=sqrt(SQR(vec_w.x)+SQR(vec_w.y)+SQR(vec_w.z));

      Dab.x=posA.x-posB.x;
      Dab.y=posA.y-posB.y;
      Dab.z=posA.z-posB.z;
      Dab=ApplyBoundaryCondition(Dab);

      Dcb.x=posC.x-posB.x;
      Dcb.y=posC.y-posB.y;
      Dcb.z=posC.z-posB.z;
      Dcb=ApplyBoundaryCondition(Dcb);
      rbc=sqrt(SQR(Dcb.x)+SQR(Dcb.y)+SQR(Dcb.z));
      Dcb.x/=rbc; Dcb.y/=rbc; Dcb.z/=rbc;

      Ddc.x=posD.x-posC.x;
      Ddc.y=posD.y-posC.y;
      Ddc.z=posD.z-posC.z;
      Ddc=ApplyBoundaryCondition(Ddc);

      dot_ab=Dab.x*Dcb.x+Dab.y*Dcb.y+Dab.z*Dcb.z;
      dot_cd=Ddc.x*Dcb.x+Ddc.y*Dcb.y+Ddc.z*Dcb.z;

      dr.x=Dab.x-dot_ab*Dcb.x;
      dr.y=Dab.y-dot_ab*Dcb.y;
      dr.z=Dab.z-dot_ab*Dcb.z;
      r=sqrt(SQR(dr.x)+SQR(dr.y)+SQR(dr.z));
      dr.x/=r; dr.y/=r; dr.z/=r;

      ds.x=Ddc.x-dot_cd*Dcb.x;
      ds.y=Ddc.y-dot_cd*Dcb.y;
      ds.z=Ddc.z-dot_cd*Dcb.z;
      s=sqrt(SQR(ds.x)+SQR(ds.y)+SQR(ds.z));
      ds.x/=s; ds.y/=s; ds.z/=s;

      // compute Cos(Phi)
      // Phi is defined in protein convention Phi(trans)=Pi
      CosPhi=dr.x*ds.x+dr.y*ds.y+dr.z*ds.z;

      // Ensure CosPhi is between -1 and 1.
      CosPhi=SIGN(MIN2(fabs(CosPhi),(REAL)1.0),CosPhi);
      CosPhi2=SQR(CosPhi);

      switch(Framework[CurrentSystem].TorsionType[f1][i])
      {
        case HARMONIC_DIHEDRAL:
          // (1/2)*p_0*(phi-p_1)^2
          // ===============================================
          // p_0/k_B [K/rad^2]
          // p_1     [degrees]
          // potential defined in terms of 'phi' and therefore contains a singularity
          // the sign of the angle-phi is positive if (Rab x Rbc) x (Rbc x Rcd) is in the
          // same direction as Rbc, and negative otherwise
          Pb.x=Dab.z*Dcb.y-Dab.y*Dcb.z;
          Pb.y=Dab.x*Dcb.z-Dab.z*Dcb.x;
          Pb.z=Dab.y*Dcb.x-Dab.x*Dcb.y;
          Pc.x=Dcb.y*Ddc.z-Dcb.z*Ddc.y;
          Pc.y=Dcb.z*Ddc.x-Dcb.x*Ddc.z;
          Pc.z=Dcb.x*Ddc.y-Dcb.y*Ddc.x;
          sign=(Dcb.x*(Pc.z*Pb.y-Pc.y*Pb.z)+Dcb.y*(Pb.z*Pc.x-Pb.x*Pc.z)
                +Dcb.z*(Pc.y*Pb.x-Pc.x*Pb.y));
          Phi=SIGN(acos(CosPhi),sign);
          SinPhi=sin(Phi);
          Phi-=parms[1];
          Phi-=NINT(Phi/(2.0*M_PI))*2.0*M_PI;
          SinPhi=SIGN(MAX2((REAL)1.0e-8,fabs(SinPhi)),SinPhi); // remove singularity
          U=0.5*parms[0]*SQR(Phi);
          DF=-parms[0]*Phi/SinPhi;
          DDF=-parms[0]*(Phi*CosPhi-SinPhi)/CUBE(SinPhi);
          break;
        case HARMONIC_COSINE_DIHEDRAL:
          // (1/2)*p_0*(cos(phi)-cos(p_1))^2
          // ===============================================
          // p_0/k_B [K]
          // p_1     [degrees]
          U=0.5*parms[0]*SQR(CosPhi-parms[1]);
          DF=parms[0]*(CosPhi-parms[1]);
          DDF=parms[0];
          break;
        case THREE_COSINE_DIHEDRAL:
          // (1/2)*p_0*(1+cos(phi))+(1/2)*p_1*(1-cos(2*phi))+(1/2)*p_2*(1+cos(3*phi))
          // ========================================================================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          U=0.5*parms[0]*(1.0+CosPhi)+parms[1]*(1.0-CosPhi2)+0.5*parms[2]*(1.0-3.0*CosPhi+4.0*CosPhi*CosPhi2);
          DF=0.5*parms[0]-2.0*parms[1]*CosPhi+1.5*parms[2]*(4.0*CosPhi2-1.0);
          DDF=-2.0*(parms[1]-6.0*parms[2]*CosPhi);
          break;
        case MM3_DIHEDRAL:
          // (1/2)*p_0*(1+cos(phi))+(1/2)*p_1*(1-cos(2*phi))+(1/2)*p_2*(1+cos(3*phi))
          // ========================================================================
          // p_0     [kcal/mol]
          // p_1     [kcal/mol]
          // p_2     [kcal/mol]
          U=0.5*parms[0]*(1.0+CosPhi)+parms[1]*(1.0-CosPhi2)+0.5*parms[2]*(1.0-3.0*CosPhi+4.0*CosPhi*CosPhi2);
          DF=0.5*parms[0]-2.0*parms[1]*CosPhi+1.5*parms[2]*(4.0*CosPhi2-1.0);
          DDF=-2.0*(parms[1]-6.0*parms[2]*CosPhi);
          break;
        case CFF_DIHEDRAL:
          // p_0*(1-cos(phi))+p_1*(1-cos(2*phi))+p_2*(1-cos(3*phi))
          // ======================================================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          U=parms[0]*(1.0-CosPhi)+2.0*parms[1]*(1.0-CosPhi2)+parms[2]*(1.0+3.0*CosPhi-4.0*CosPhi*CosPhi2);
          DF=-parms[0]-4.0*parms[1]*CosPhi+3.0*parms[2]*(1.0-4.0*CosPhi2);
          DDF=-4.0*(parms[1]+6.0*parms[2]*CosPhi);
          break;
        case CFF_DIHEDRAL2:
          // p_0*(1+cos(phi))+p_1*(1+cos(2*phi))+p_2*(1+cos(3*phi))
          // ======================================================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          U=parms[0]*(1.0+CosPhi)+parms[2]+CosPhi*(-3.0*parms[2]+2.0*CosPhi*(parms[1]+2.0*parms[2]*CosPhi));
          DF=parms[0]-3.0*parms[2]+4.0*CosPhi*(parms[1]+3.0*parms[2]*CosPhi);
          DDF=4.0*(parms[1]+6.0*parms[2]*CosPhi);
          break;
        case SIX_COSINE_DIHEDRAL:
          // Prod_i=0^5 p_i*cos(phi)^i
          // =========================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          // p_3/k_B [K]
          // p_4/k_B [K]
          // p_5/k_B [K]
          // the Ryckaert-Bellemans potentials is often used for alkanes, the use implies exclusion of VDW-interactions
          // between the first and last atoms of the dihedral, and Phi'=Phi-pi is defined accoording to the
          // polymer convention Phi'(trans)=0.
          U=parms[0]-parms[1]*CosPhi+parms[2]*CosPhi2-parms[3]*CosPhi*CosPhi2+
                 parms[4]*SQR(CosPhi2)-parms[5]*SQR(CosPhi2)*CosPhi;
          DF=-parms[1]+2.0*parms[2]*CosPhi-3.0*parms[3]*CosPhi2+4.0*parms[4]*CosPhi2*CosPhi-5.0*parms[5]*SQR(CosPhi2);
          DDF=2.0*parms[2]-6.0*parms[3]*CosPhi+12.0*parms[4]*CosPhi2-20.0*parms[5]*CosPhi2*CosPhi;
          break;
        case TRAPPE_DIHEDRAL:
          // p_0[0]+p_1*(1+cos(phi))+p_2*(1-cos(2*phi))+p_3*(1+cos(3*phi))
          // =============================================================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          // p_3/k_B [K]
          U=parms[0]+(1.0+CosPhi)*(parms[1]+parms[3]-2.0*(CosPhi-1.0)*(parms[2]-2.0*parms[3]*CosPhi));
          DF=parms[1]-4.0*parms[2]*CosPhi+3.0*parms[3]*(4.0*CosPhi2-1.0);
          DDF=-4.0*(parms[2]-6.0*parms[3]*CosPhi);
          break;
        case CVFF_DIHEDRAL:
          // p_0*(1+cos(p_1*phi-p_2))
          // ========================
          // p_0/k_B [K]
          // p_1     [-]
          // p_2     [degrees]
          // potential defined in terms of 'phi' and therefore contains a singularity
          // the sign of the angle-phi is positive if (Rab x Rbc) x (Rbc x Rcd) is in the
          // same direction as Dbc, and negative otherwise
          Pb.x=Dab.z*Dcb.y-Dab.y*Dcb.z;
          Pb.y=Dab.x*Dcb.z-Dab.z*Dcb.x;
          Pb.z=Dab.y*Dcb.x-Dab.x*Dcb.y;
          Pc.x=Dcb.y*Ddc.z-Dcb.z*Ddc.y;
          Pc.y=Dcb.z*Ddc.x-Dcb.x*Ddc.z;
          Pc.z=Dcb.x*Ddc.y-Dcb.y*Ddc.x;
          sign=(Dcb.x*(Pc.z*Pb.y-Pc.y*Pb.z)+Dcb.y*(Pb.z*Pc.x-Pb.x*Pc.z)
                +Dcb.z*(Pc.y*Pb.x-Pc.x*Pb.y));
          Phi=SIGN(acos(CosPhi),sign);
          SinPhi=sin(Phi);
          U=parms[0]*(1.0+cos(parms[1]*Phi-parms[2]));
          if(fabs(SinPhi)<1.0e-8)
          {
            SinPhi=SIGN(MAX2((REAL)1.0e-8,fabs(SinPhi)),SinPhi);  // remove singularity
            DF=parms[0]*parms[1]*cos(parms[2])*SIGN(parms[1],sin(parms[1]*Phi)*Phi)+parms[0]*parms[1]*cos(parms[1]*Phi)*sin(parms[2])/SinPhi;
          }
          else
            DF=(parms[0]*parms[1]*sin(parms[1]*Phi-parms[2]))/SinPhi;
          DDF=-parms[0]*parms[1]*(parms[1]*cos(-parms[1]*Phi+parms[2])+sin(-parms[1]*Phi+parms[2])*CosPhi/SinPhi)/SQR(SinPhi);
          break;
        case OPLS_DIHEDRAL:
          // (1/2)p_0[0]+(1/2)p_1*(1+cos(phi))+(1/2)p_2*(1-cos(2*phi))+(1/2)p_3*(1+cos(3*phi))
          // =================================================================================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          // p_3/k_B [K]
          U=0.5*(parms[0]+(1.0+CosPhi)*(parms[1]+parms[3]-2.0*(CosPhi-1.0)*(parms[2]-2.0*parms[3]*CosPhi)));
          DF=0.5*parms[1]-2.0*parms[2]*CosPhi+1.5*parms[3]*(4.0*CosPhi2-1.0);
          DDF=-2.0*(parms[2]-6.0*parms[3]*CosPhi);
          break;
        case FOURIER_SERIES_DIHEDRAL:
          // (1/2)p_0*(1+cos(phi))+(1/2)p_1(1-cos(2*phi))+(1/2)*p2_2*(1+cos(3*phi))+
          // (1/2)p_3*(1-cos(4*phi))+(1/2)p_4*(1+cos(5*phi))+(1/2)p_5*(1+cos(6*phi))
          // =======================================================================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          // p_3/k_B [K]
          // p_4/k_B [K]
          // p_5/k_B [K]
          U=0.5*(parms[0]+2.0*parms[1]+parms[2]+parms[4]+2.0*parms[5]+(parms[0]-3.0*parms[2]+5.0*parms[4])*CosPhi-
                 2.0*(parms[1]-4.0*parms[3]+9.0*parms[5])*CosPhi2+4.0*(parms[2]-5.0*parms[4])*CosPhi2*CosPhi-
                 8.0*(parms[3]-6.0*parms[5])*SQR(CosPhi2)+16.0*parms[4]*SQR(CosPhi2)*CosPhi-32.0*parms[5]*CUBE(CosPhi2));
          DF=0.5*(parms[0]-3.0*parms[2]+5.0*parms[4])-2.0*(parms[1]-4.0*parms[3]+9.0*parms[5])*CosPhi+
             6.0*(parms[2]-5.0*parms[4])*CosPhi2-16.0*(parms[3]-6.0*parms[5])*CosPhi2*CosPhi+40.0*parms[4]*SQR(CosPhi2)-96.0*parms[5]*CosPhi2*CUBE(CosPhi);
          DDF=-2.0*parms[1]+8.0*parms[3]-18.0*parms[5]+12.0*(parms[2]-5.0*parms[4])*CosPhi
              -48.0*(parms[3]-6.0*parms[5])*CosPhi2+160.0*parms[4]*CosPhi2*CosPhi-480.0*parms[5]*SQR(CosPhi2);
          break;
        case FOURIER_SERIES_DIHEDRAL2:
          // (1/2)p_0*(1+cos(phi))+(1/2)p_1(1-cos(2*phi))+(1/2)*p2_2*(1+cos(3*phi))+
          // (1/2)p_3*(1+cos(4*phi))+(1/2)p_4*(1+cos(5*phi))+(1/2)p_5*(1+cos(6*phi))
          // =======================================================================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          // p_3/k_B [K]
          // p_4/k_B [K]
          // p_5/k_B [K]
          U=0.5*(parms[2]+2.0*parms[3]+parms[4]-3.0*parms[2]*CosPhi+5.0*parms[4]*CosPhi+parms[0]*(1.0+CosPhi)+
            2.0*(parms[1]-parms[1]*CosPhi2+CosPhi2*(parms[5]*SQR(3.0-4.0*CosPhi2)+4.0*parms[3]*(CosPhi2-1.0)+
            2.0*CosPhi*(parms[2]+parms[4]*(4.0*CosPhi2-5.0)))));
          DF=0.5*parms[0]+parms[2]*(6.0*CosPhi2-1.5)+parms[4]*(2.5-30.0*CosPhi2+40.0*SQR(CosPhi2))+
             CosPhi*(-2.0*parms[1]+parms[3]*(16.0*CosPhi2-8.0)+parms[5]*(18.0-96.0*CosPhi2+96.0*SQR(CosPhi2)));
          DDF=-2.0*parms[1]+12.0*parms[2]*CosPhi+parms[3]*(48.0*CosPhi2-8.0)+parms[4]*CosPhi*(160.0*CosPhi2-60.0)
              +parms[5]*(18.0-288.0*CosPhi2+480.0*SQR(CosPhi2));
          break;
        default:
          printf("Undefined Torsion potential in 'framework_hessian.c'\n");
          U=DF=DDF=0.0;
          exit(0);
          break;
      }

      *Energy+=U;

      if((index_i<0)&&(index_j<0)&&(index_k<0)&&(index_l<0)) continue;

     // Calculate the first derivative vectors.
      d=dot_ab/rbc;
      e=dot_cd/rbc;

      dtA.x=(ds.x-CosPhi*dr.x)/r;
      dtA.y=(ds.y-CosPhi*dr.y)/r;
      dtA.z=(ds.z-CosPhi*dr.z)/r;

      dtD.x=(dr.x-CosPhi*ds.x)/s;
      dtD.y=(dr.y-CosPhi*ds.y)/s;
      dtD.z=(dr.z-CosPhi*ds.z)/s;

      dtB.x=dtA.x*(d-1.0)+e*dtD.x;
      dtB.y=dtA.y*(d-1.0)+e*dtD.y;
      dtB.z=dtA.z*(d-1.0)+e*dtD.z;

      dtC.x=-dtD.x*(e+1.0)-d*dtA.x;
      dtC.y=-dtD.y*(e+1.0)-d*dtA.y;
      dtC.z=-dtD.z*(e+1.0)-d*dtA.z;

      // forces are oppositely directed to the gradient
      fa.x=DF*dtA.x;
      fa.y=DF*dtA.y;
      fa.z=DF*dtA.z;

      fb.x=DF*dtB.x;
      fb.y=DF*dtB.y;
      fb.z=DF*dtB.z;

      fc.x=DF*dtC.x;
      fc.y=DF*dtC.y;
      fc.z=DF*dtC.z;

      fd.x=DF*dtD.x;
      fd.y=DF*dtD.y;
      fd.z=DF*dtD.z;

      // add contribution to the strain derivative tensor
      // Note: rbc is here because the vector was normalized before
      S.ax=Dab.x*fa.x+Dcb.x*rbc*(fc.x+fd.x)+Ddc.x*fd.x;
      S.bx=Dab.y*fa.x+Dcb.y*rbc*(fc.x+fd.x)+Ddc.y*fd.x;
      S.cx=Dab.z*fa.x+Dcb.z*rbc*(fc.x+fd.x)+Ddc.z*fd.x;

      S.ay=Dab.x*fa.y+Dcb.x*rbc*(fc.y+fd.y)+Ddc.x*fd.y;
      S.by=Dab.y*fa.y+Dcb.y*rbc*(fc.y+fd.y)+Ddc.y*fd.y;
      S.cy=Dab.z*fa.y+Dcb.z*rbc*(fc.y+fd.y)+Ddc.z*fd.y;

      S.az=Dab.x*fa.z+Dcb.x*rbc*(fc.z+fd.z)+Ddc.x*fd.z;
      S.bz=Dab.y*fa.z+Dcb.y*rbc*(fc.z+fd.z)+Ddc.y*fd.z;
      S.cz=Dab.z*fa.z+Dcb.z*rbc*(fc.z+fd.z)+Ddc.z*fd.z;

      StrainDerivative->ax+=S.ax;
      StrainDerivative->bx+=S.bx;
      StrainDerivative->cx+=S.cx;

      StrainDerivative->ay+=S.ay;
      StrainDerivative->by+=S.by;
      StrainDerivative->cy+=S.cy;

      StrainDerivative->az+=S.az;
      StrainDerivative->bz+=S.bz;
      StrainDerivative->cz+=S.cz;

      if(ComputeGradient)
      {
        if(index_i>=0)
        {
          Gradient[index_i]+=fa.x;
          Gradient[index_i+1]+=fa.y;
          Gradient[index_i+2]+=fa.z;
        }

        if(index_j>=0)
        {
          Gradient[index_j]+=fb.x;
          Gradient[index_j+1]+=fb.y;
          Gradient[index_j+2]+=fb.z;
        }

        if(index_k>=0)
        {
          Gradient[index_k]+=fc.x;
          Gradient[index_k+1]+=fc.y;
          Gradient[index_k+2]+=fc.z;
        }

        if(index_l>=0)
        {
          Gradient[index_l]+=fd.x;
          Gradient[index_l+1]+=fd.y;
          Gradient[index_l+2]+=fd.z;
        }

        GradientStrainTorsion(Gradient,S);
      }

      if(ComputeHessian)
      {
        // Calculate the derivatives of DOTIJ and DOTLK.
        DIL.x=DF*Dcb.x/rbc;
        DIL.y=DF*Dcb.y/rbc;
        DIL.z=DF*Dcb.z/rbc;
        DDJ.x=DF*((2.0*d-1.0)*Dcb.x-Dab.x/rbc)/rbc;
        DDJ.y=DF*((2.0*d-1.0)*Dcb.y-Dab.y/rbc)/rbc;
        DDJ.z=DF*((2.0*d-1.0)*Dcb.z-Dab.z/rbc)/rbc;
        DDK.x=-DF*(2.0*d*Dcb.x-Dab.x/rbc)/rbc;
        DDK.y=-DF*(2.0*d*Dcb.y-Dab.y/rbc)/rbc;
        DDK.z=-DF*(2.0*d*Dcb.z-Dab.z/rbc)/rbc;
        DEJ.x=DF*(2.0*e*Dcb.x-Ddc.x/rbc)/rbc;
        DEJ.y=DF*(2.0*e*Dcb.y-Ddc.y/rbc)/rbc;
        DEJ.z=DF*(2.0*e*Dcb.z-Ddc.z/rbc)/rbc;
        DEK.x=-DF*((2.0*e+1.0)*Dcb.x-Ddc.x/rbc)/rbc;
        DEK.y=-DF*((2.0*e+1.0)*Dcb.y-Ddc.y/rbc)/rbc;
        DEK.z=-DF*((2.0*e+1.0)*Dcb.z-Ddc.z/rbc)/rbc;

        // Calculate some diagonal Hessian terms for I.
        D2I.ax=DF*(CosPhi*(dr.x*dr.x+Dcb.x*Dcb.x-1.0)/r-2.0*dr.x*dtA.x)/r;
        D2I.by=DF*(CosPhi*(dr.y*dr.y+Dcb.y*Dcb.y-1.0)/r-2.0*dr.y*dtA.y)/r;
        D2I.cz=DF*(CosPhi*(dr.z*dr.z+Dcb.z*Dcb.z-1.0)/r-2.0*dr.z*dtA.z)/r;
        D2I.ay=DF*(CosPhi*(dr.x*dr.y+Dcb.x*Dcb.y)/r-dr.x*dtA.y-dr.y*dtA.x)/r;
        D2I.az=DF*(CosPhi*(dr.x*dr.z+Dcb.x*Dcb.z)/r-dr.x*dtA.z-dr.z*dtA.x)/r;
        D2I.bz=DF*(CosPhi*(dr.y*dr.z+Dcb.y*Dcb.z)/r-dr.y*dtA.z-dr.z*dtA.y)/r;

        // Calculate some diagonal Hessian terms for L.
        D2L.ax=DF*(CosPhi*(ds.x*ds.x+Dcb.x*Dcb.x-1.0)/s-2.0*ds.x*dtD.x)/s;
        D2L.by=DF*(CosPhi*(ds.y*ds.y+Dcb.y*Dcb.y-1.0)/s-2.0*ds.y*dtD.y)/s;
        D2L.cz=DF*(CosPhi*(ds.z*ds.z+Dcb.z*Dcb.z-1.0)/s-2.0*ds.z*dtD.z)/s;
        D2L.ay=DF*(CosPhi*(ds.x*ds.y+Dcb.x*Dcb.y)/s-ds.x*dtD.y-ds.y*dtD.x)/s;
        D2L.az=DF*(CosPhi*(ds.x*ds.z+Dcb.x*Dcb.z)/s-ds.x*dtD.z-ds.z*dtD.x)/s;
        D2L.bz=DF*(CosPhi*(ds.y*ds.z+Dcb.y*Dcb.z)/s-ds.y*dtD.z-ds.z*dtD.y)/s;

        // Calculate the IL off-diagonal terms.
        D2IL.ax=DF*(CosPhi*dr.x*ds.x-dr.x*dr.x-ds.x*ds.x-Dcb.x*Dcb.x+1.0)/(r*s);
        D2IL.ay=DF*(CosPhi*dr.x*ds.y-dr.x*dr.y-ds.x*ds.y-Dcb.x*Dcb.y)/(r*s);
        D2IL.az=DF*(CosPhi*dr.x*ds.z-dr.x*dr.z-ds.x*ds.z-Dcb.x*Dcb.z)/(r*s);
        D2IL.bx=DF*(CosPhi*dr.y*ds.x-dr.y*dr.x-ds.y*ds.x-Dcb.y*Dcb.x)/(r*s);
        D2IL.by=DF*(CosPhi*dr.y*ds.y-dr.y*dr.y-ds.y*ds.y-Dcb.y*Dcb.y+1.0)/(r*s);
        D2IL.bz=DF*(CosPhi*dr.y*ds.z-dr.y*dr.z-ds.y*ds.z-Dcb.y*Dcb.z)/(r*s);
        D2IL.cx=DF*(CosPhi*dr.z*ds.x-dr.z*dr.x-ds.z*ds.x-Dcb.z*Dcb.x)/(r*s);
        D2IL.cy=DF*(CosPhi*dr.z*ds.y-dr.z*dr.y-ds.z*ds.y-Dcb.z*Dcb.y)/(r*s);
        D2IL.cz=DF*(CosPhi*dr.z*ds.z-dr.z*dr.z-ds.z*ds.z-Dcb.z*Dcb.z+1.0)/(r*s);

        // Calculate the IJ off-diagonal terms.
        D2IJ.ax=D2I.ax*(d-1.0)+D2IL.ax*e+DIL.x*dtA.x;
        D2IJ.ay=D2I.ay*(d-1.0)+D2IL.ay*e+DIL.x*dtA.y;
        D2IJ.az=D2I.az*(d-1.0)+D2IL.az*e+DIL.x*dtA.z;
        D2IJ.bx=D2I.ay*(d-1.0)+D2IL.bx*e+DIL.y*dtA.x;
        D2IJ.by=D2I.by*(d-1.0)+D2IL.by*e+DIL.y*dtA.y;
        D2IJ.bz=D2I.bz*(d-1.0)+D2IL.bz*e+DIL.y*dtA.z;
        D2IJ.cx=D2I.az*(d-1.0)+D2IL.cx*e+DIL.z*dtA.x;
        D2IJ.cy=D2I.bz*(d-1.0)+D2IL.cy*e+DIL.z*dtA.y;
        D2IJ.cz=D2I.cz*(d-1.0)+D2IL.cz*e+DIL.z*dtA.z;

        // Calculate the IK off-diagonal terms.
        D2IK.ax=-D2I.ax*d-D2IL.ax*(e+1.0)-DIL.x*dtA.x;
        D2IK.ay=-D2I.ay*d-D2IL.ay*(e+1.0)-DIL.x*dtA.y;
        D2IK.az=-D2I.az*d-D2IL.az*(e+1.0)-DIL.x*dtA.z;
        D2IK.bx=-D2I.ay*d-D2IL.bx*(e+1.0)-DIL.y*dtA.x;
        D2IK.by=-D2I.by*d-D2IL.by*(e+1.0)-DIL.y*dtA.y;
        D2IK.bz=-D2I.bz*d-D2IL.bz*(e+1.0)-DIL.y*dtA.z;
        D2IK.cx=-D2I.az*d-D2IL.cx*(e+1.0)-DIL.z*dtA.x;
        D2IK.cy=-D2I.bz*d-D2IL.cy*(e+1.0)-DIL.z*dtA.y;
        D2IK.cz=-D2I.cz*d-D2IL.cz*(e+1.0)-DIL.z*dtA.z;

        // Calculate the JL off-diagonal terms.
        D2JL.ax=D2IL.ax*(d-1.0)+D2L.ax*e+DIL.x*dtD.x;
        D2JL.ay=D2IL.ay*(d-1.0)+D2L.ay*e+DIL.y*dtD.x;
        D2JL.az=D2IL.az*(d-1.0)+D2L.az*e+DIL.z*dtD.x;
        D2JL.bx=D2IL.bx*(d-1.0)+D2L.ay*e+DIL.x*dtD.y;
        D2JL.by=D2IL.by*(d-1.0)+D2L.by*e+DIL.y*dtD.y;
        D2JL.bz=D2IL.bz*(d-1.0)+D2L.bz*e+DIL.z*dtD.y;
        D2JL.cx=D2IL.cx*(d-1.0)+D2L.az*e+DIL.x*dtD.z;
        D2JL.cy=D2IL.cy*(d-1.0)+D2L.bz*e+DIL.y*dtD.z;
        D2JL.cz=D2IL.cz*(d-1.0)+D2L.cz*e+DIL.z*dtD.z;

        // Calculate the KL off-diagonal terms.
        D2KL.ax=-D2IL.ax*d-D2L.ax*(e+1.0)-DIL.x*dtD.x;
        D2KL.ay=-D2IL.ay*d-D2L.ay*(e+1.0)-DIL.y*dtD.x;
        D2KL.az=-D2IL.az*d-D2L.az*(e+1.0)-DIL.z*dtD.x;
        D2KL.bx=-D2IL.bx*d-D2L.ay*(e+1.0)-DIL.x*dtD.y;
        D2KL.by=-D2IL.by*d-D2L.by*(e+1.0)-DIL.y*dtD.y;
        D2KL.bz=-D2IL.bz*d-D2L.bz*(e+1.0)-DIL.z*dtD.y;
        D2KL.cx=-D2IL.cx*d-D2L.az*(e+1.0)-DIL.x*dtD.z;
        D2KL.cy=-D2IL.cy*d-D2L.bz*(e+1.0)-DIL.y*dtD.z;
        D2KL.cz=-D2IL.cz*d-D2L.cz*(e+1.0)-DIL.z*dtD.z;

        // Calculate the JJ diagonal terms.
        D2J.ax=D2IJ.ax*(d-1.0)+D2JL.ax*e+DDJ.x*dtA.x+DEJ.x*dtD.x;
        D2J.by=D2IJ.by*(d-1.0)+D2JL.by*e+DDJ.y*dtA.y+DEJ.y*dtD.y;
        D2J.cz=D2IJ.cz*(d-1.0)+D2JL.cz*e+DDJ.z*dtA.z+DEJ.z*dtD.z;
        D2J.ay=D2IJ.ay*(d-1.0)+D2JL.bx*e+DDJ.y*dtA.x+DEJ.y*dtD.x;
        D2J.az=D2IJ.az*(d-1.0)+D2JL.cx*e+DDJ.z*dtA.x+DEJ.z*dtD.x;
        D2J.bz=D2IJ.bz*(d-1.0)+D2JL.cy*e+DDJ.z*dtA.y+DEJ.z*dtD.y;

        // Calculate the KK diagonal terms.
        D2K.ax=-D2KL.ax*(e+1.0)-D2IK.ax*d-DDK.x*dtA.x-DEK.x*dtD.x;
        D2K.by=-D2KL.by*(e+1.0)-D2IK.by*d-DDK.y*dtA.y-DEK.y*dtD.y;
        D2K.cz=-D2KL.cz*(e+1.0)-D2IK.cz*d-DDK.z*dtA.z-DEK.z*dtD.z;
        D2K.ay=-D2KL.ay*(e+1.0)-D2IK.bx*d-DDK.x*dtA.y-DEK.x*dtD.y;
        D2K.az=-D2KL.az*(e+1.0)-D2IK.cx*d-DDK.x*dtA.z-DEK.x*dtD.z;
        D2K.bz=-D2KL.bz*(e+1.0)-D2IK.cy*d-DDK.y*dtA.z-DEK.y*dtD.z;

        // Calculate the JK off-diagonal terms.
        D2JK.ax=-D2IJ.ax*d-D2JL.ax*(e+1.0)-DDJ.x*dtA.x-DEJ.x*dtD.x;
        D2JK.ay=-D2IJ.bx*d-D2JL.ay*(e+1.0)-DDJ.x*dtA.y-DEJ.x*dtD.y;
        D2JK.az=-D2IJ.cx*d-D2JL.az*(e+1.0)-DDJ.x*dtA.z-DEJ.x*dtD.z;
        D2JK.bx=-D2IJ.ay*d-D2JL.bx*(e+1.0)-DDJ.y*dtA.x-DEJ.y*dtD.x;
        D2JK.by=-D2IJ.by*d-D2JL.by*(e+1.0)-DDJ.y*dtA.y-DEJ.y*dtD.y;
        D2JK.bz=-D2IJ.cy*d-D2JL.bz*(e+1.0)-DDJ.y*dtA.z-DEJ.y*dtD.z;
        D2JK.cx=-D2IJ.az*d-D2JL.cx*(e+1.0)-DDJ.z*dtA.x-DEJ.z*dtD.x;
        D2JK.cy=-D2IJ.bz*d-D2JL.cy*(e+1.0)-DDJ.z*dtA.y-DEJ.z*dtD.y;
        D2JK.cz=-D2IJ.cz*d-D2JL.cz*(e+1.0)-DDJ.z*dtA.z-DEJ.z*dtD.z;

        // Calculate the diagonal blocks of the hessian.
        if(index_i>=0)
        {
          HessianMatrix.element[index_i][index_i].re+=DDF*dtA.x*dtA.x+D2I.ax;
          HessianMatrix.element[index_i][index_i+1].re+=DDF*dtA.x*dtA.y+D2I.ay;
          HessianMatrix.element[index_i+1][index_i+1].re+=DDF*dtA.y*dtA.y+D2I.by;
          HessianMatrix.element[index_i][index_i+2].re+=DDF*dtA.x*dtA.z+D2I.az;
          HessianMatrix.element[index_i+1][index_i+2].re+=DDF*dtA.y*dtA.z+D2I.bz;
          HessianMatrix.element[index_i+2][index_i+2].re+=DDF*dtA.z*dtA.z+D2I.cz;
        }

        if(index_j>=0)
        {
          HessianMatrix.element[index_j][index_j].re+=DDF*dtB.x*dtB.x+D2J.ax;
          HessianMatrix.element[index_j][index_j+1].re+=DDF*dtB.x*dtB.y+D2J.ay;
          HessianMatrix.element[index_j+1][index_j+1].re+=DDF*dtB.y*dtB.y+D2J.by;
          HessianMatrix.element[index_j][index_j+2].re+=DDF*dtB.x*dtB.z+D2J.az;
          HessianMatrix.element[index_j+1][index_j+2].re+=DDF*dtB.y*dtB.z+D2J.bz;
          HessianMatrix.element[index_j+2][index_j+2].re+=DDF*dtB.z*dtB.z+D2J.cz;
        }

        if(index_k>=0)
        {
          HessianMatrix.element[index_k][index_k].re+=DDF*dtC.x*dtC.x+D2K.ax;
          HessianMatrix.element[index_k][index_k+1].re+=DDF*dtC.x*dtC.y+D2K.ay;
          HessianMatrix.element[index_k+1][index_k+1].re+=DDF*dtC.y*dtC.y+D2K.by;
          HessianMatrix.element[index_k][index_k+2].re+=DDF*dtC.x*dtC.z+D2K.az;
          HessianMatrix.element[index_k+1][index_k+2].re+=DDF*dtC.y*dtC.z+D2K.bz;
          HessianMatrix.element[index_k+2][index_k+2].re+=DDF*dtC.z*dtC.z+D2K.cz;
        }

        if(index_l>=0)
        {
          HessianMatrix.element[index_l][index_l].re+=DDF*dtD.x*dtD.x+D2L.ax;
          HessianMatrix.element[index_l][index_l+1].re+=DDF*dtD.x*dtD.y+D2L.ay;
          HessianMatrix.element[index_l+1][index_l+1].re+=DDF*dtD.y*dtD.y+D2L.by;
          HessianMatrix.element[index_l][index_l+2].re+=DDF*dtD.x*dtD.z+D2L.az;
          HessianMatrix.element[index_l+1][index_l+2].re+=DDF*dtD.y*dtD.z+D2L.bz;
          HessianMatrix.element[index_l+2][index_l+2].re+=DDF*dtD.z*dtD.z+D2L.cz;
        }

        if((index_i>=0)&&(index_j>=0))
        {
          if(index_i<index_j)
          {
            HessianMatrix.element[index_i][index_j].re+=DDF*dtA.x*dtB.x+D2IJ.ax;
            HessianMatrix.element[index_i][index_j+1].re+=DDF*dtA.x*dtB.y+D2IJ.ay;
            HessianMatrix.element[index_i][index_j+2].re+=DDF*dtA.x*dtB.z+D2IJ.az;
            HessianMatrix.element[index_i+1][index_j].re+=DDF*dtA.y*dtB.x+D2IJ.bx;
            HessianMatrix.element[index_i+1][index_j+1].re+=DDF*dtA.y*dtB.y+D2IJ.by;
            HessianMatrix.element[index_i+1][index_j+2].re+=DDF*dtA.y*dtB.z+D2IJ.bz;
            HessianMatrix.element[index_i+2][index_j].re+=DDF*dtA.z*dtB.x+D2IJ.cx;
            HessianMatrix.element[index_i+2][index_j+1].re+=DDF*dtA.z*dtB.y+D2IJ.cy;
            HessianMatrix.element[index_i+2][index_j+2].re+=DDF*dtA.z*dtB.z+D2IJ.cz;
          }
          else
          {
            HessianMatrix.element[index_j][index_i].re+=DDF*dtA.x*dtB.x+D2IJ.ax;
            HessianMatrix.element[index_j][index_i+1].re+=DDF*dtA.y*dtB.x+D2IJ.bx;
            HessianMatrix.element[index_j][index_i+2].re+=DDF*dtA.z*dtB.x+D2IJ.cx;
            HessianMatrix.element[index_j+1][index_i].re+=DDF*dtA.x*dtB.y+D2IJ.ay;
            HessianMatrix.element[index_j+1][index_i+1].re+=DDF*dtA.y*dtB.y+D2IJ.by;
            HessianMatrix.element[index_j+1][index_i+2].re+=DDF*dtA.z*dtB.y+D2IJ.cy;
            HessianMatrix.element[index_j+2][index_i].re+=DDF*dtA.x*dtB.z+D2IJ.az;
            HessianMatrix.element[index_j+2][index_i+1].re+=DDF*dtA.y*dtB.z+D2IJ.bz;
            HessianMatrix.element[index_j+2][index_i+2].re+=DDF*dtA.z*dtB.z+D2IJ.cz;
          }
        }

        if((index_i>=0)&&(index_k>=0))
        {
          if(index_i<index_k)
          {
            HessianMatrix.element[index_i][index_k].re+=DDF*dtA.x*dtC.x+D2IK.ax;
            HessianMatrix.element[index_i][index_k+1].re+=DDF*dtA.x*dtC.y+D2IK.ay;
            HessianMatrix.element[index_i][index_k+2].re+=DDF*dtA.x*dtC.z+D2IK.az;
            HessianMatrix.element[index_i+1][index_k].re+=DDF*dtA.y*dtC.x+D2IK.bx;
            HessianMatrix.element[index_i+1][index_k+1].re+=DDF*dtA.y*dtC.y+D2IK.by;
            HessianMatrix.element[index_i+1][index_k+2].re+=DDF*dtA.y*dtC.z+D2IK.bz;
            HessianMatrix.element[index_i+2][index_k].re+=DDF*dtA.z*dtC.x+D2IK.cx;
            HessianMatrix.element[index_i+2][index_k+1].re+=DDF*dtA.z*dtC.y+D2IK.cy;
            HessianMatrix.element[index_i+2][index_k+2].re+=DDF*dtA.z*dtC.z+D2IK.cz;
          }
          else
          {
            HessianMatrix.element[index_k][index_i].re+=DDF*dtA.x*dtC.x+D2IK.ax;
            HessianMatrix.element[index_k][index_i+1].re+=DDF*dtA.y*dtC.x+D2IK.bx;
            HessianMatrix.element[index_k][index_i+2].re+=DDF*dtA.z*dtC.x+D2IK.cx;
            HessianMatrix.element[index_k+1][index_i].re+=DDF*dtA.x*dtC.y+D2IK.ay;
            HessianMatrix.element[index_k+1][index_i+1].re+=DDF*dtA.y*dtC.y+D2IK.by;
            HessianMatrix.element[index_k+1][index_i+2].re+=DDF*dtA.z*dtC.y+D2IK.cy;
            HessianMatrix.element[index_k+2][index_i].re+=DDF*dtA.x*dtC.z+D2IK.az;
            HessianMatrix.element[index_k+2][index_i+1].re+=DDF*dtA.y*dtC.z+D2IK.bz;
            HessianMatrix.element[index_k+2][index_i+2].re+=DDF*dtA.z*dtC.z+D2IK.cz;
          }
        }

        if((index_i>=0)&&(index_l>=0))
        {
          if(index_i<index_l)
          {
            HessianMatrix.element[index_i][index_l].re+=DDF*dtA.x*dtD.x+D2IL.ax;
            HessianMatrix.element[index_i][index_l+1].re+=DDF*dtA.x*dtD.y+D2IL.ay; 
            HessianMatrix.element[index_i][index_l+2].re+=DDF*dtA.x*dtD.z+D2IL.az; 
            HessianMatrix.element[index_i+1][index_l].re+=DDF*dtA.y*dtD.x+D2IL.bx; 
            HessianMatrix.element[index_i+1][index_l+1].re+=DDF*dtA.y*dtD.y+D2IL.by;
            HessianMatrix.element[index_i+1][index_l+2].re+=DDF*dtA.y*dtD.z+D2IL.bz;
            HessianMatrix.element[index_i+2][index_l].re+=DDF*dtA.z*dtD.x+D2IL.cx;
            HessianMatrix.element[index_i+2][index_l+1].re+=DDF*dtA.z*dtD.y+D2IL.cy;
            HessianMatrix.element[index_i+2][index_l+2].re+=DDF*dtA.z*dtD.z+D2IL.cz;
          }
          else
          {
            HessianMatrix.element[index_l][index_i].re+=DDF*dtA.x*dtD.x+D2IL.ax;
            HessianMatrix.element[index_l][index_i+1].re+=DDF*dtA.y*dtD.x+D2IL.bx;
            HessianMatrix.element[index_l][index_i+2].re+=DDF*dtA.z*dtD.x+D2IL.cx;
            HessianMatrix.element[index_l+1][index_i].re+=DDF*dtA.x*dtD.y+D2IL.ay;
            HessianMatrix.element[index_l+1][index_i+1].re+=DDF*dtA.y*dtD.y+D2IL.by;
            HessianMatrix.element[index_l+1][index_i+2].re+=DDF*dtA.z*dtD.y+D2IL.cy;
            HessianMatrix.element[index_l+2][index_i].re+=DDF*dtA.x*dtD.z+D2IL.az;
            HessianMatrix.element[index_l+2][index_i+1].re+=DDF*dtA.y*dtD.z+D2IL.bz;
            HessianMatrix.element[index_l+2][index_i+2].re+=DDF*dtA.z*dtD.z+D2IL.cz;
          }
        }

        if((index_j>=0)&&(index_k>=0))
        {
          if(index_j<index_k)
          {
            HessianMatrix.element[index_j][index_k].re+=DDF*dtB.x*dtC.x+D2JK.ax;
            HessianMatrix.element[index_j][index_k+1].re+=DDF*dtB.x*dtC.y+D2JK.ay; 
            HessianMatrix.element[index_j][index_k+2].re+=DDF*dtB.x*dtC.z+D2JK.az;
            HessianMatrix.element[index_j+1][index_k].re+=DDF*dtB.y*dtC.x+D2JK.bx;
            HessianMatrix.element[index_j+1][index_k+1].re+=DDF*dtB.y*dtC.y+D2JK.by;
            HessianMatrix.element[index_j+1][index_k+2].re+=DDF*dtB.y*dtC.z+D2JK.bz;
            HessianMatrix.element[index_j+2][index_k].re+=DDF*dtB.z*dtC.x+D2JK.cx;
            HessianMatrix.element[index_j+2][index_k+1].re+=DDF*dtB.z*dtC.y+D2JK.cy;
            HessianMatrix.element[index_j+2][index_k+2].re+=DDF*dtB.z*dtC.z+D2JK.cz;
          }
          else
          {
            HessianMatrix.element[index_k][index_j].re+=DDF*dtB.x*dtC.x+D2JK.ax;
            HessianMatrix.element[index_k][index_j+1].re+=DDF*dtB.y*dtC.x+D2JK.bx;
            HessianMatrix.element[index_k][index_j+2].re+=DDF*dtB.z*dtC.x+D2JK.cx;
            HessianMatrix.element[index_k+1][index_j].re+=DDF*dtB.x*dtC.y+D2JK.ay;
            HessianMatrix.element[index_k+1][index_j+1].re+=DDF*dtB.y*dtC.y+D2JK.by;
            HessianMatrix.element[index_k+1][index_j+2].re+=DDF*dtB.z*dtC.y+D2JK.cy;
            HessianMatrix.element[index_k+2][index_j].re+=DDF*dtB.x*dtC.z+D2JK.az;
            HessianMatrix.element[index_k+2][index_j+1].re+=DDF*dtB.y*dtC.z+D2JK.bz;
            HessianMatrix.element[index_k+2][index_j+2].re+=DDF*dtB.z*dtC.z+D2JK.cz;
          }
        }

        if((index_j>=0)&&(index_l>=0))
        {
          if(index_j<index_l)
          {
            HessianMatrix.element[index_j][index_l].re+=DDF*dtB.x*dtD.x+D2JL.ax;
            HessianMatrix.element[index_j][index_l+1].re+=DDF*dtB.x*dtD.y+D2JL.ay;
            HessianMatrix.element[index_j][index_l+2].re+=DDF*dtB.x*dtD.z+D2JL.az;
            HessianMatrix.element[index_j+1][index_l].re+=DDF*dtB.y*dtD.x+D2JL.bx;
            HessianMatrix.element[index_j+1][index_l+1].re+=DDF*dtB.y*dtD.y+D2JL.by;
            HessianMatrix.element[index_j+1][index_l+2].re+=DDF*dtB.y*dtD.z+D2JL.bz;
            HessianMatrix.element[index_j+2][index_l].re+=DDF*dtB.z*dtD.x+D2JL.cx;
            HessianMatrix.element[index_j+2][index_l+1].re+=DDF*dtB.z*dtD.y+D2JL.cy;
            HessianMatrix.element[index_j+2][index_l+2].re+=DDF*dtB.z*dtD.z+D2JL.cz;
          }
          else
          {
            HessianMatrix.element[index_l][index_j].re+=DDF*dtB.x*dtD.x+D2JL.ax;
            HessianMatrix.element[index_l][index_j+1].re+=DDF*dtB.y*dtD.x+D2JL.bx;
            HessianMatrix.element[index_l][index_j+2].re+=DDF*dtB.z*dtD.x+D2JL.cx;
            HessianMatrix.element[index_l+1][index_j].re+=DDF*dtB.x*dtD.y+D2JL.ay;
            HessianMatrix.element[index_l+1][index_j+1].re+=DDF*dtB.y*dtD.y+D2JL.by;
            HessianMatrix.element[index_l+1][index_j+2].re+=DDF*dtB.z*dtD.y+D2JL.cy;
            HessianMatrix.element[index_l+2][index_j].re+=DDF*dtB.x*dtD.z+D2JL.az;
            HessianMatrix.element[index_l+2][index_j+1].re+=DDF*dtB.y*dtD.z+D2JL.bz;
            HessianMatrix.element[index_l+2][index_j+2].re+=DDF*dtB.z*dtD.z+D2JL.cz;
          }
        }

        if((index_k>=0)&&(index_l>=0))
        {
          if(index_k<index_l)
          {
            HessianMatrix.element[index_k][index_l].re+=DDF*dtC.x*dtD.x+D2KL.ax;
            HessianMatrix.element[index_k][index_l+1].re+=DDF*dtC.x*dtD.y+D2KL.ay;
            HessianMatrix.element[index_k][index_l+2].re+=DDF*dtC.x*dtD.z+D2KL.az;
            HessianMatrix.element[index_k+1][index_l].re+=DDF*dtC.y*dtD.x+D2KL.bx;
            HessianMatrix.element[index_k+1][index_l+1].re+=DDF*dtC.y*dtD.y+D2KL.by;
            HessianMatrix.element[index_k+1][index_l+2].re+=DDF*dtC.y*dtD.z+D2KL.bz;
            HessianMatrix.element[index_k+2][index_l].re+=DDF*dtC.z*dtD.x+D2KL.cx;
            HessianMatrix.element[index_k+2][index_l+1].re+=DDF*dtC.z*dtD.y+D2KL.cy;
            HessianMatrix.element[index_k+2][index_l+2].re+=DDF*dtC.z*dtD.z+D2KL.cz;
          }
          else
          {
            HessianMatrix.element[index_l][index_k].re+=DDF*dtC.x*dtD.x+D2KL.ax;
            HessianMatrix.element[index_l][index_k+1].re+=DDF*dtC.y*dtD.x+D2KL.bx;
            HessianMatrix.element[index_l][index_k+2].re+=DDF*dtC.z*dtD.x+D2KL.cx;
            HessianMatrix.element[index_l+1][index_k].re+=DDF*dtC.x*dtD.y+D2KL.ay;
            HessianMatrix.element[index_l+1][index_k+1].re+=DDF*dtC.y*dtD.y+D2KL.by;
            HessianMatrix.element[index_l+1][index_k+2].re+=DDF*dtC.z*dtD.y+D2KL.cy;
            HessianMatrix.element[index_l+2][index_k].re+=DDF*dtC.x*dtD.z+D2KL.az;
            HessianMatrix.element[index_l+2][index_k+1].re+=DDF*dtC.y*dtD.z+D2KL.bz;
            HessianMatrix.element[index_l+2][index_k+2].re+=DDF*dtC.z*dtD.z+D2KL.cz;
          }
        }
      }
    }
  }
}

void ComputeFrameworkImproperTorsionPhonon(VECTOR k,REAL *Energy,REAL* Gradient,COMPLEX_MATRIX HessianMatrix,REAL_MATRIX3x3 *StrainDerivative,int ComputeGradient,int ComputeHessian)
{
  int i,A,B,C,D,f1;
  POINT posA,posB,posC,posD;
  REAL d,e,rbc;
  VECTOR Dab,Dcb,Ddc,dr,ds;
  REAL dot_ab,dot_cd,r,s,sign;
  REAL U,CosPhi,Phi,CosPhi2,SinPhi;
  VECTOR dtA,dtB,dtC,dtD,Pb,Pc;
  REAL *parms;
  int index_i,index_j,index_k,index_l;
  REAL_MATRIX3x3 D2I,D2L,D2IL,D2IJ,D2IK,D2JL;
  REAL_MATRIX3x3 D2KL,D2J,D2K,D2JK,S;
  VECTOR DIL,DDJ,DDK,DEJ,DEK;
  REAL DF,DDF;
  REAL u,w,v;
  VECTOR vec_u,vec_v,vec_w;
  VECTOR fa,fb,fc,fd;

  for(f1=0;f1<Framework[CurrentSystem].NumberOfFrameworks;f1++)
  {
    for(i=0;i<Framework[CurrentSystem].NumberOfImproperTorsions[f1];i++)
    {
      A=Framework[CurrentSystem].ImproperTorsions[f1][i].A;
      B=Framework[CurrentSystem].ImproperTorsions[f1][i].B;
      C=Framework[CurrentSystem].ImproperTorsions[f1][i].C;
      D=Framework[CurrentSystem].ImproperTorsions[f1][i].D;
      parms=Framework[CurrentSystem].ImproperTorsionArguments[f1][i];

      posA=Framework[CurrentSystem].Atoms[f1][A].Position;
      posB=Framework[CurrentSystem].Atoms[f1][B].Position;
      posC=Framework[CurrentSystem].Atoms[f1][C].Position;
      posD=Framework[CurrentSystem].Atoms[f1][D].Position;

      index_i=Framework[CurrentSystem].Atoms[f1][A].HessianIndex;
      index_j=Framework[CurrentSystem].Atoms[f1][B].HessianIndex;
      index_k=Framework[CurrentSystem].Atoms[f1][C].HessianIndex;
      index_l=Framework[CurrentSystem].Atoms[f1][D].HessianIndex;

      vec_u.x=posB.x-posC.x;
      vec_u.y=posB.y-posC.y;
      vec_u.z=posB.z-posC.z;
      vec_u=ApplyBoundaryCondition(vec_u);
      u=sqrt(SQR(vec_u.x)+SQR(vec_u.y)+SQR(vec_u.z));

      vec_v.x=posD.x-posC.x;
      vec_v.y=posD.y-posC.y;
      vec_v.z=posD.z-posC.z;
      vec_v=ApplyBoundaryCondition(vec_v);
      v=sqrt(SQR(vec_v.x)+SQR(vec_v.y)+SQR(vec_v.z));

      vec_w.x=posA.x-posB.x;
      vec_w.y=posA.y-posB.y;
      vec_w.z=posA.z-posB.z;
      vec_w=ApplyBoundaryCondition(vec_w);
      w=sqrt(SQR(vec_w.x)+SQR(vec_w.y)+SQR(vec_w.z));

      Dab.x=posA.x-posB.x;
      Dab.y=posA.y-posB.y;
      Dab.z=posA.z-posB.z;
      Dab=ApplyBoundaryCondition(Dab);

      Dcb.x=posC.x-posB.x;
      Dcb.y=posC.y-posB.y;
      Dcb.z=posC.z-posB.z;
      Dcb=ApplyBoundaryCondition(Dcb);
      rbc=sqrt(SQR(Dcb.x)+SQR(Dcb.y)+SQR(Dcb.z));
      Dcb.x/=rbc; Dcb.y/=rbc; Dcb.z/=rbc;

      Ddc.x=posD.x-posC.x;
      Ddc.y=posD.y-posC.y;
      Ddc.z=posD.z-posC.z;
      Ddc=ApplyBoundaryCondition(Ddc);

      dot_ab=Dab.x*Dcb.x+Dab.y*Dcb.y+Dab.z*Dcb.z;
      dot_cd=Ddc.x*Dcb.x+Ddc.y*Dcb.y+Ddc.z*Dcb.z;

      dr.x=Dab.x-dot_ab*Dcb.x;
      dr.y=Dab.y-dot_ab*Dcb.y;
      dr.z=Dab.z-dot_ab*Dcb.z;
      r=sqrt(SQR(dr.x)+SQR(dr.y)+SQR(dr.z));
      dr.x/=r; dr.y/=r; dr.z/=r;

      ds.x=Ddc.x-dot_cd*Dcb.x;
      ds.y=Ddc.y-dot_cd*Dcb.y;
      ds.z=Ddc.z-dot_cd*Dcb.z;
      s=sqrt(SQR(ds.x)+SQR(ds.y)+SQR(ds.z));
      ds.x/=s; ds.y/=s; ds.z/=s;

      // compute Cos(Phi)
      // Phi is defined in protein convention Phi(trans)=Pi
      CosPhi=dr.x*ds.x+dr.y*ds.y+dr.z*ds.z;

      // Ensure CosPhi is between -1 and 1.
      CosPhi=SIGN(MIN2(fabs(CosPhi),(REAL)1.0),CosPhi);
      CosPhi2=SQR(CosPhi);

      switch(Framework[CurrentSystem].ImproperTorsionType[f1][i])
      {
        case HARMONIC_IMPROPER_DIHEDRAL:
          // (1/2)*p_0*(phi-p_1)^2
          // ===============================================
          // p_0/k_B [K/rad^2]
          // p_1     [degrees]
          // potential defined in terms of 'phi' and therefore contains a singularity
          // the sign of the angle-phi is positive if (Rab x Rbc) x (Rbc x Rcd) is in the
          // same direction as Rbc, and negative otherwise
          Pb.x=Dab.z*Dcb.y-Dab.y*Dcb.z;
          Pb.y=Dab.x*Dcb.z-Dab.z*Dcb.x;
          Pb.z=Dab.y*Dcb.x-Dab.x*Dcb.y;
          Pc.x=Dcb.y*Ddc.z-Dcb.z*Ddc.y;
          Pc.y=Dcb.z*Ddc.x-Dcb.x*Ddc.z;
          Pc.z=Dcb.x*Ddc.y-Dcb.y*Ddc.x;
          sign=(Dcb.x*(Pc.z*Pb.y-Pc.y*Pb.z)+Dcb.y*(Pb.z*Pc.x-Pb.x*Pc.z)
                +Dcb.z*(Pc.y*Pb.x-Pc.x*Pb.y));
          Phi=SIGN(acos(CosPhi),sign);
          SinPhi=sin(Phi);
          Phi-=parms[1];
          Phi-=NINT(Phi/(2.0*M_PI))*2.0*M_PI;
          SinPhi=SIGN(MAX2((REAL)1.0e-8,fabs(SinPhi)),SinPhi); // remove singularity
          U=0.5*parms[0]*SQR(Phi);
          DF=-parms[0]*Phi/SinPhi;
          DDF=-parms[0]*(Phi*CosPhi-SinPhi)/CUBE(SinPhi);
          break;
        case HARMONIC_COSINE_IMPROPER_DIHEDRAL:
          // (1/2)*p_0*(cos(phi)-cos(p_1))^2
          // ===============================================
          // p_0/k_B [K]
          // p_1     [degrees]
          U=0.5*parms[0]*SQR(CosPhi-parms[1]);
          DF=parms[0]*(CosPhi-parms[1]);
          DDF=parms[0];
          break;
        case THREE_COSINE_IMPROPER_DIHEDRAL:
          // (1/2)*p_0*(1+cos(phi))+(1/2)*p_1*(1-cos(2*phi))+(1/2)*p_2*(1+cos(3*phi))
          // ========================================================================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          U=0.5*parms[0]*(1.0+CosPhi)+parms[1]*(1.0-CosPhi2)+0.5*parms[2]*(1.0-3.0*CosPhi+4.0*CosPhi*CosPhi2);
          DF=0.5*parms[0]-2.0*parms[1]*CosPhi+1.5*parms[2]*(4.0*CosPhi2-1.0);
          DDF=-2.0*(parms[1]-6.0*parms[2]*CosPhi);
          break;
        case MM3_IMPROPER_DIHEDRAL:
          // (1/2)*p_0*(1+cos(phi))+(1/2)*p_1*(1-cos(2*phi))+(1/2)*p_2*(1+cos(3*phi))
          // ========================================================================
          // p_0     [kcal/mol]
          // p_1     [kcal/mol]
          // p_2     [kcal/mol]
          U=0.5*parms[0]*(1.0+CosPhi)+parms[1]*(1.0-CosPhi2)+0.5*parms[2]*(1.0-3.0*CosPhi+4.0*CosPhi*CosPhi2);
          DF=0.5*parms[0]-2.0*parms[1]*CosPhi+1.5*parms[2]*(4.0*CosPhi2-1.0);
          DDF=-2.0*(parms[1]-6.0*parms[2]*CosPhi);
          break;
        case CFF_IMPROPER_DIHEDRAL:
          // p_0*(1-cos(phi))+p_1*(1-cos(2*phi))+p_2*(1-cos(3*phi))
          // ======================================================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          U=parms[0]*(1.0-CosPhi)+2.0*parms[1]*(1.0-CosPhi2)+parms[2]*(1.0+3.0*CosPhi-4.0*CosPhi*CosPhi2);
          DF=-parms[0]-4.0*parms[1]*CosPhi+3.0*parms[2]*(1.0-4.0*CosPhi2);
          DDF=-4.0*(parms[1]+6.0*parms[2]*CosPhi);
          break;
        case CFF_IMPROPER_DIHEDRAL2:
          // p_0*(1+cos(phi))+p_1*(1+cos(2*phi))+p_2*(1+cos(3*phi))
          // ======================================================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          U=parms[0]*(1.0+CosPhi)+parms[2]+CosPhi*(-3.0*parms[2]+2.0*CosPhi*(parms[1]+2.0*parms[2]*CosPhi));
          DF=parms[0]-3.0*parms[2]+4.0*CosPhi*(parms[1]+3.0*parms[2]*CosPhi);
          DDF=4.0*(parms[1]+6.0*parms[2]*CosPhi);
          break;
        case SIX_COSINE_IMPROPER_DIHEDRAL:
          // Prod_i=0^5 p_i*cos(phi)^i
          // =========================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          // p_3/k_B [K]
          // p_4/k_B [K]
          // p_5/k_B [K]
          // the Ryckaert-Bellemans potentials is often used for alkanes, the use implies exclusion of VDW-interactions
          // between the first and last atoms of the dihedral, and Phi'=Phi-pi is defined accoording to the
          // polymer convention Phi'(trans)=0.
          U=parms[0]-parms[1]*CosPhi+parms[2]*CosPhi2-parms[3]*CosPhi*CosPhi2+
                 parms[4]*SQR(CosPhi2)-parms[5]*SQR(CosPhi2)*CosPhi;
          DF=-parms[1]+2.0*parms[2]*CosPhi-3.0*parms[3]*CosPhi2+4.0*parms[4]*CosPhi2*CosPhi-5.0*parms[5]*SQR(CosPhi2);
          DDF=2.0*parms[2]-6.0*parms[3]*CosPhi+12.0*parms[4]*CosPhi2-20.0*parms[5]*CosPhi2*CosPhi;
          break;
        case TRAPPE_IMPROPER_DIHEDRAL:
          U=parms[0]+(1.0+CosPhi)*(parms[1]+parms[3]-2.0*(CosPhi-1.0)*(parms[2]-2.0*parms[3]*CosPhi));
          DF=parms[1]-4.0*parms[2]*CosPhi+3.0*parms[3]*(4.0*CosPhi2-1.0);
          DDF=-4.0*(parms[2]-6.0*parms[3]*CosPhi);
          // p_0[0]+p_1*(1+cos(phi))+p_2*(1-cos(2*phi))+p_3*(1+cos(3*phi))
          // =============================================================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          // p_3/k_B [K]
          break;
        case CVFF_IMPROPER_DIHEDRAL:
          // p_0*(1+cos(p_1*phi-p_2))
          // ========================
          // p_0/k_B [K]
          // p_1     [-]
          // p_2     [degrees]
          // potential defined in terms of 'phi' and therefore contains a singularity
          // the sign of the angle-phi is positive if (Rab x Rbc) x (Rbc x Rcd) is in the
          // same direction as Dbc, and negative otherwise
          Pb.x=Dab.z*Dcb.y-Dab.y*Dcb.z;
          Pb.y=Dab.x*Dcb.z-Dab.z*Dcb.x;
          Pb.z=Dab.y*Dcb.x-Dab.x*Dcb.y;
          Pc.x=Dcb.y*Ddc.z-Dcb.z*Ddc.y;
          Pc.y=Dcb.z*Ddc.x-Dcb.x*Ddc.z;
          Pc.z=Dcb.x*Ddc.y-Dcb.y*Ddc.x;
          sign=(Dcb.x*(Pc.z*Pb.y-Pc.y*Pb.z)+Dcb.y*(Pb.z*Pc.x-Pb.x*Pc.z)
                +Dcb.z*(Pc.y*Pb.x-Pc.x*Pb.y));
          Phi=SIGN(acos(CosPhi),sign);
          SinPhi=sin(Phi);
          U=parms[0]*(1.0+cos(parms[1]*Phi-parms[2]));
          if(fabs(SinPhi)<1.0e-8)
          {
            SinPhi=SIGN(MAX2((REAL)1.0e-8,fabs(SinPhi)),SinPhi);  // remove singularity
            DF=parms[0]*parms[1]*cos(parms[2])*SIGN(parms[1],sin(parms[1]*Phi)*Phi)+parms[0]*parms[1]*cos(parms[1]*Phi)*sin(parms[2])/SinPhi;
          }
          else
            DF=(parms[0]*parms[1]*sin(parms[1]*Phi-parms[2]))/SinPhi;
          DDF=-parms[0]*parms[1]*(parms[1]*cos(-parms[1]*Phi+parms[2])+sin(-parms[1]*Phi+parms[2])*CosPhi/SinPhi)/SQR(SinPhi);
          break;
        case OPLS_IMPROPER_DIHEDRAL:
          // (1/2)p_0[0]+(1/2)p_1*(1+cos(phi))+(1/2)p_2*(1-cos(2*phi))+(1/2)p_3*(1+cos(3*phi))
          // =================================================================================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          // p_3/k_B [K]
          U=0.5*(parms[0]+(1.0+CosPhi)*(parms[1]+parms[3]-2.0*(CosPhi-1.0)*(parms[2]-2.0*parms[3]*CosPhi)));
          DF=0.5*parms[1]-2.0*parms[2]*CosPhi+1.5*parms[3]*(4.0*CosPhi2-1.0);
          DDF=-2.0*(parms[2]-6.0*parms[3]*CosPhi);
          break;
        case FOURIER_SERIES_IMPROPER_DIHEDRAL:
          // (1/2)p_0*(1+cos(phi))+(1/2)p_1(1-cos(2*phi))+(1/2)*p2_2*(1+cos(3*phi))+
          // (1/2)p_3*(1-cos(4*phi))+(1/2)p_4*(1+cos(5*phi))+(1/2)p_5*(1+cos(6*phi))
          // =======================================================================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          // p_3/k_B [K]
          // p_4/k_B [K]
          // p_5/k_B [K]
          U=0.5*(parms[0]+2.0*parms[1]+parms[2]+parms[4]+2.0*parms[5]+(parms[0]-3.0*parms[2]+5.0*parms[4])*CosPhi-
                 2.0*(parms[1]-4.0*parms[3]+9.0*parms[5])*CosPhi2+4.0*(parms[2]-5.0*parms[4])*CosPhi2*CosPhi-
                 8.0*(parms[3]-6.0*parms[5])*SQR(CosPhi2)+16.0*parms[4]*SQR(CosPhi2)*CosPhi-32.0*parms[5]*CUBE(CosPhi2));
          DF=0.5*(parms[0]-3.0*parms[2]+5.0*parms[4])-2.0*(parms[1]-4.0*parms[3]+9.0*parms[5])*CosPhi+
             6.0*(parms[2]-5.0*parms[4])*CosPhi2-16.0*(parms[3]-6.0*parms[5])*CosPhi2*CosPhi+40.0*parms[4]*SQR(CosPhi2)-96.0*parms[5]*CosPhi2*CUBE(CosPhi);
          DDF=-2.0*parms[1]+8.0*parms[3]-18.0*parms[5]+12.0*(parms[2]-5.0*parms[4])*CosPhi
              -48.0*(parms[3]-6.0*parms[5])*CosPhi2+160.0*parms[4]*CosPhi2*CosPhi-480.0*parms[5]*SQR(CosPhi2);
          break;
        case FOURIER_SERIES_IMPROPER_DIHEDRAL2:
          // (1/2)p_0*(1+cos(phi))+(1/2)p_1(1-cos(2*phi))+(1/2)*p2_2*(1+cos(3*phi))+
          // (1/2)p_3*(1+cos(4*phi))+(1/2)p_4*(1+cos(5*phi))+(1/2)p_5*(1+cos(6*phi))
          // =======================================================================
          // p_0/k_B [K]
          // p_1/k_B [K]
          // p_2/k_B [K]
          // p_3/k_B [K]
          // p_4/k_B [K]
          // p_5/k_B [K]
          U=0.5*(parms[2]+2.0*parms[3]+parms[4]-3.0*parms[2]*CosPhi+5.0*parms[4]*CosPhi+parms[0]*(1.0+CosPhi)+
            2.0*(parms[1]-parms[1]*CosPhi2+CosPhi2*(parms[5]*SQR(3.0-4.0*CosPhi2)+4.0*parms[3]*(CosPhi2-1.0)+
            2.0*CosPhi*(parms[2]+parms[4]*(4.0*CosPhi2-5.0)))));
          DF=0.5*parms[0]+parms[2]*(6.0*CosPhi2-1.5)+parms[4]*(2.5-30.0*CosPhi2+40.0*SQR(CosPhi2))+
             CosPhi*(-2.0*parms[1]+parms[3]*(16.0*CosPhi2-8.0)+parms[5]*(18.0-96.0*CosPhi2+96.0*SQR(CosPhi2)));
          DDF=-2.0*parms[1]+12.0*parms[2]*CosPhi+parms[3]*(48.0*CosPhi2-8.0)+parms[4]*CosPhi*(160.0*CosPhi2-60.0)
              +parms[5]*(18.0-288.0*CosPhi2+480.0*SQR(CosPhi2));
          break;
        case FIXED_IMPROPER_DIHEDRAL:
          U=DF=DDF=0.0;
          break;
        default:
          printf("Undefined Improper Torsion potential in 'framework_hessian.c'\n");
          U=DF=DDF=0.0;
          exit(0);
          break;
      }

      *Energy+=U;

      if((index_i<0)&&(index_j<0)&&(index_k<0)&&(index_l<0)) continue;

     // Calculate the first derivative vectors.
      d=dot_ab/rbc;
      e=dot_cd/rbc;

      dtA.x=(ds.x-CosPhi*dr.x)/r;
      dtA.y=(ds.y-CosPhi*dr.y)/r;
      dtA.z=(ds.z-CosPhi*dr.z)/r;

      dtD.x=(dr.x-CosPhi*ds.x)/s;
      dtD.y=(dr.y-CosPhi*ds.y)/s;
      dtD.z=(dr.z-CosPhi*ds.z)/s;

      dtB.x=dtA.x*(d-1.0)+e*dtD.x;
      dtB.y=dtA.y*(d-1.0)+e*dtD.y;
      dtB.z=dtA.z*(d-1.0)+e*dtD.z;

      dtC.x=-dtD.x*(e+1.0)-d*dtA.x;
      dtC.y=-dtD.y*(e+1.0)-d*dtA.y;
      dtC.z=-dtD.z*(e+1.0)-d*dtA.z;

      // forces are oppositely directed to the gradient
      fa.x=DF*dtA.x;
      fa.y=DF*dtA.y;
      fa.z=DF*dtA.z;

      fb.x=DF*dtB.x;
      fb.y=DF*dtB.y;
      fb.z=DF*dtB.z;

      fc.x=DF*dtC.x;
      fc.y=DF*dtC.y;
      fc.z=DF*dtC.z;

      fd.x=DF*dtD.x;
      fd.y=DF*dtD.y;
      fd.z=DF*dtD.z;

      // add contribution to the strain derivative tensor
      // Note: rbc is here because the vector was normalized before
      S.ax=Dab.x*fa.x+Dcb.x*rbc*(fc.x+fd.x)+Ddc.x*fd.x;
      S.bx=Dab.y*fa.x+Dcb.y*rbc*(fc.x+fd.x)+Ddc.y*fd.x;
      S.cx=Dab.z*fa.x+Dcb.z*rbc*(fc.x+fd.x)+Ddc.z*fd.x;

      S.ay=Dab.x*fa.y+Dcb.x*rbc*(fc.y+fd.y)+Ddc.x*fd.y;
      S.by=Dab.y*fa.y+Dcb.y*rbc*(fc.y+fd.y)+Ddc.y*fd.y;
      S.cy=Dab.z*fa.y+Dcb.z*rbc*(fc.y+fd.y)+Ddc.z*fd.y;

      S.az=Dab.x*fa.z+Dcb.x*rbc*(fc.z+fd.z)+Ddc.x*fd.z;
      S.bz=Dab.y*fa.z+Dcb.y*rbc*(fc.z+fd.z)+Ddc.y*fd.z;
      S.cz=Dab.z*fa.z+Dcb.z*rbc*(fc.z+fd.z)+Ddc.z*fd.z;

      StrainDerivative->ax+=S.ax;
      StrainDerivative->bx+=S.bx;
      StrainDerivative->cx+=S.cx;

      StrainDerivative->ay+=S.ay;
      StrainDerivative->by+=S.by;
      StrainDerivative->cy+=S.cy;

      StrainDerivative->az+=S.az;
      StrainDerivative->bz+=S.bz;
      StrainDerivative->cz+=S.cz;

      if(ComputeGradient)
      {
        if(index_i>=0)
        {
          Gradient[index_i]+=fa.x;
          Gradient[index_i+1]+=fa.y;
          Gradient[index_i+2]+=fa.z;
        }

        if(index_j>=0)
        {
          Gradient[index_j]+=fb.x;
          Gradient[index_j+1]+=fb.y;
          Gradient[index_j+2]+=fb.z;
        }

        if(index_k>=0)
        {
          Gradient[index_k]+=fc.x;
          Gradient[index_k+1]+=fc.y;
          Gradient[index_k+2]+=fc.z;
        }

        if(index_l>=0)
        {
          Gradient[index_l]+=fd.x;
          Gradient[index_l+1]+=fd.y;
          Gradient[index_l+2]+=fd.z;
        }

        GradientStrainTorsion(Gradient,S);
      }

      if(ComputeHessian)
      {
        // Calculate the derivatives of DOTIJ and DOTLK.
        DIL.x=DF*Dcb.x/rbc;
        DIL.y=DF*Dcb.y/rbc;
        DIL.z=DF*Dcb.z/rbc;
        DDJ.x=DF*((2.0*d-1.0)*Dcb.x-Dab.x/rbc)/rbc;
        DDJ.y=DF*((2.0*d-1.0)*Dcb.y-Dab.y/rbc)/rbc;
        DDJ.z=DF*((2.0*d-1.0)*Dcb.z-Dab.z/rbc)/rbc;
        DDK.x=-DF*(2.0*d*Dcb.x-Dab.x/rbc)/rbc;
        DDK.y=-DF*(2.0*d*Dcb.y-Dab.y/rbc)/rbc;
        DDK.z=-DF*(2.0*d*Dcb.z-Dab.z/rbc)/rbc;
        DEJ.x=DF*(2.0*e*Dcb.x-Ddc.x/rbc)/rbc;
        DEJ.y=DF*(2.0*e*Dcb.y-Ddc.y/rbc)/rbc;
        DEJ.z=DF*(2.0*e*Dcb.z-Ddc.z/rbc)/rbc;
        DEK.x=-DF*((2.0*e+1.0)*Dcb.x-Ddc.x/rbc)/rbc;
        DEK.y=-DF*((2.0*e+1.0)*Dcb.y-Ddc.y/rbc)/rbc;
        DEK.z=-DF*((2.0*e+1.0)*Dcb.z-Ddc.z/rbc)/rbc;

        // Calculate some diagonal Hessian terms for I.
        D2I.ax=DF*(CosPhi*(dr.x*dr.x+Dcb.x*Dcb.x-1.0)/r-2.0*dr.x*dtA.x)/r;
        D2I.by=DF*(CosPhi*(dr.y*dr.y+Dcb.y*Dcb.y-1.0)/r-2.0*dr.y*dtA.y)/r;
        D2I.cz=DF*(CosPhi*(dr.z*dr.z+Dcb.z*Dcb.z-1.0)/r-2.0*dr.z*dtA.z)/r;
        D2I.ay=DF*(CosPhi*(dr.x*dr.y+Dcb.x*Dcb.y)/r-dr.x*dtA.y-dr.y*dtA.x)/r;
        D2I.az=DF*(CosPhi*(dr.x*dr.z+Dcb.x*Dcb.z)/r-dr.x*dtA.z-dr.z*dtA.x)/r;
        D2I.bz=DF*(CosPhi*(dr.y*dr.z+Dcb.y*Dcb.z)/r-dr.y*dtA.z-dr.z*dtA.y)/r;

        // Calculate some diagonal Hessian terms for L.
        D2L.ax=DF*(CosPhi*(ds.x*ds.x+Dcb.x*Dcb.x-1.0)/s-2.0*ds.x*dtD.x)/s;
        D2L.by=DF*(CosPhi*(ds.y*ds.y+Dcb.y*Dcb.y-1.0)/s-2.0*ds.y*dtD.y)/s;
        D2L.cz=DF*(CosPhi*(ds.z*ds.z+Dcb.z*Dcb.z-1.0)/s-2.0*ds.z*dtD.z)/s;
        D2L.ay=DF*(CosPhi*(ds.x*ds.y+Dcb.x*Dcb.y)/s-ds.x*dtD.y-ds.y*dtD.x)/s;
        D2L.az=DF*(CosPhi*(ds.x*ds.z+Dcb.x*Dcb.z)/s-ds.x*dtD.z-ds.z*dtD.x)/s;
        D2L.bz=DF*(CosPhi*(ds.y*ds.z+Dcb.y*Dcb.z)/s-ds.y*dtD.z-ds.z*dtD.y)/s;

        // Calculate the IL off-diagonal terms.
        D2IL.ax=DF*(CosPhi*dr.x*ds.x-dr.x*dr.x-ds.x*ds.x-Dcb.x*Dcb.x+1.0)/(r*s);
        D2IL.ay=DF*(CosPhi*dr.x*ds.y-dr.x*dr.y-ds.x*ds.y-Dcb.x*Dcb.y)/(r*s);
        D2IL.az=DF*(CosPhi*dr.x*ds.z-dr.x*dr.z-ds.x*ds.z-Dcb.x*Dcb.z)/(r*s);
        D2IL.bx=DF*(CosPhi*dr.y*ds.x-dr.y*dr.x-ds.y*ds.x-Dcb.y*Dcb.x)/(r*s);
        D2IL.by=DF*(CosPhi*dr.y*ds.y-dr.y*dr.y-ds.y*ds.y-Dcb.y*Dcb.y+1.0)/(r*s);
        D2IL.bz=DF*(CosPhi*dr.y*ds.z-dr.y*dr.z-ds.y*ds.z-Dcb.y*Dcb.z)/(r*s);
        D2IL.cx=DF*(CosPhi*dr.z*ds.x-dr.z*dr.x-ds.z*ds.x-Dcb.z*Dcb.x)/(r*s);
        D2IL.cy=DF*(CosPhi*dr.z*ds.y-dr.z*dr.y-ds.z*ds.y-Dcb.z*Dcb.y)/(r*s);
        D2IL.cz=DF*(CosPhi*dr.z*ds.z-dr.z*dr.z-ds.z*ds.z-Dcb.z*Dcb.z+1.0)/(r*s);

        // Calculate the IJ off-diagonal terms.
        D2IJ.ax=D2I.ax*(d-1.0)+D2IL.ax*e+DIL.x*dtA.x;
        D2IJ.ay=D2I.ay*(d-1.0)+D2IL.ay*e+DIL.x*dtA.y;
        D2IJ.az=D2I.az*(d-1.0)+D2IL.az*e+DIL.x*dtA.z;
        D2IJ.bx=D2I.ay*(d-1.0)+D2IL.bx*e+DIL.y*dtA.x;
        D2IJ.by=D2I.by*(d-1.0)+D2IL.by*e+DIL.y*dtA.y;
        D2IJ.bz=D2I.bz*(d-1.0)+D2IL.bz*e+DIL.y*dtA.z;
        D2IJ.cx=D2I.az*(d-1.0)+D2IL.cx*e+DIL.z*dtA.x;
        D2IJ.cy=D2I.bz*(d-1.0)+D2IL.cy*e+DIL.z*dtA.y;
        D2IJ.cz=D2I.cz*(d-1.0)+D2IL.cz*e+DIL.z*dtA.z;

        // Calculate the IK off-diagonal terms.
        D2IK.ax=-D2I.ax*d-D2IL.ax*(e+1.0)-DIL.x*dtA.x;
        D2IK.ay=-D2I.ay*d-D2IL.ay*(e+1.0)-DIL.x*dtA.y;
        D2IK.az=-D2I.az*d-D2IL.az*(e+1.0)-DIL.x*dtA.z;
        D2IK.bx=-D2I.ay*d-D2IL.bx*(e+1.0)-DIL.y*dtA.x;
        D2IK.by=-D2I.by*d-D2IL.by*(e+1.0)-DIL.y*dtA.y;
        D2IK.bz=-D2I.bz*d-D2IL.bz*(e+1.0)-DIL.y*dtA.z;
        D2IK.cx=-D2I.az*d-D2IL.cx*(e+1.0)-DIL.z*dtA.x;
        D2IK.cy=-D2I.bz*d-D2IL.cy*(e+1.0)-DIL.z*dtA.y;
        D2IK.cz=-D2I.cz*d-D2IL.cz*(e+1.0)-DIL.z*dtA.z;

        // Calculate the JL off-diagonal terms.
        D2JL.ax=D2IL.ax*(d-1.0)+D2L.ax*e+DIL.x*dtD.x;
        D2JL.ay=D2IL.ay*(d-1.0)+D2L.ay*e+DIL.y*dtD.x;
        D2JL.az=D2IL.az*(d-1.0)+D2L.az*e+DIL.z*dtD.x;
        D2JL.bx=D2IL.bx*(d-1.0)+D2L.ay*e+DIL.x*dtD.y;
        D2JL.by=D2IL.by*(d-1.0)+D2L.by*e+DIL.y*dtD.y;
        D2JL.bz=D2IL.bz*(d-1.0)+D2L.bz*e+DIL.z*dtD.y;
        D2JL.cx=D2IL.cx*(d-1.0)+D2L.az*e+DIL.x*dtD.z;
        D2JL.cy=D2IL.cy*(d-1.0)+D2L.bz*e+DIL.y*dtD.z;
        D2JL.cz=D2IL.cz*(d-1.0)+D2L.cz*e+DIL.z*dtD.z;

        // Calculate the KL off-diagonal terms.
        D2KL.ax=-D2IL.ax*d-D2L.ax*(e+1.0)-DIL.x*dtD.x;
        D2KL.ay=-D2IL.ay*d-D2L.ay*(e+1.0)-DIL.y*dtD.x;
        D2KL.az=-D2IL.az*d-D2L.az*(e+1.0)-DIL.z*dtD.x;
        D2KL.bx=-D2IL.bx*d-D2L.ay*(e+1.0)-DIL.x*dtD.y;
        D2KL.by=-D2IL.by*d-D2L.by*(e+1.0)-DIL.y*dtD.y;
        D2KL.bz=-D2IL.bz*d-D2L.bz*(e+1.0)-DIL.z*dtD.y;
        D2KL.cx=-D2IL.cx*d-D2L.az*(e+1.0)-DIL.x*dtD.z;
        D2KL.cy=-D2IL.cy*d-D2L.bz*(e+1.0)-DIL.y*dtD.z;
        D2KL.cz=-D2IL.cz*d-D2L.cz*(e+1.0)-DIL.z*dtD.z;

        // Calculate the JJ diagonal terms.
        D2J.ax=D2IJ.ax*(d-1.0)+D2JL.ax*e+DDJ.x*dtA.x+DEJ.x*dtD.x;
        D2J.by=D2IJ.by*(d-1.0)+D2JL.by*e+DDJ.y*dtA.y+DEJ.y*dtD.y;
        D2J.cz=D2IJ.cz*(d-1.0)+D2JL.cz*e+DDJ.z*dtA.z+DEJ.z*dtD.z;
        D2J.ay=D2IJ.ay*(d-1.0)+D2JL.bx*e+DDJ.y*dtA.x+DEJ.y*dtD.x;
        D2J.az=D2IJ.az*(d-1.0)+D2JL.cx*e+DDJ.z*dtA.x+DEJ.z*dtD.x;
        D2J.bz=D2IJ.bz*(d-1.0)+D2JL.cy*e+DDJ.z*dtA.y+DEJ.z*dtD.y;

        // Calculate the KK diagonal terms.
        D2K.ax=-D2KL.ax*(e+1.0)-D2IK.ax*d-DDK.x*dtA.x-DEK.x*dtD.x;
        D2K.by=-D2KL.by*(e+1.0)-D2IK.by*d-DDK.y*dtA.y-DEK.y*dtD.y;
        D2K.cz=-D2KL.cz*(e+1.0)-D2IK.cz*d-DDK.z*dtA.z-DEK.z*dtD.z;
        D2K.ay=-D2KL.ay*(e+1.0)-D2IK.bx*d-DDK.x*dtA.y-DEK.x*dtD.y;
        D2K.az=-D2KL.az*(e+1.0)-D2IK.cx*d-DDK.x*dtA.z-DEK.x*dtD.z;
        D2K.bz=-D2KL.bz*(e+1.0)-D2IK.cy*d-DDK.y*dtA.z-DEK.y*dtD.z;

        // Calculate the JK off-diagonal terms.
        D2JK.ax=-D2IJ.ax*d-D2JL.ax*(e+1.0)-DDJ.x*dtA.x-DEJ.x*dtD.x;
        D2JK.ay=-D2IJ.bx*d-D2JL.ay*(e+1.0)-DDJ.x*dtA.y-DEJ.x*dtD.y;
        D2JK.az=-D2IJ.cx*d-D2JL.az*(e+1.0)-DDJ.x*dtA.z-DEJ.x*dtD.z;
        D2JK.bx=-D2IJ.ay*d-D2JL.bx*(e+1.0)-DDJ.y*dtA.x-DEJ.y*dtD.x;
        D2JK.by=-D2IJ.by*d-D2JL.by*(e+1.0)-DDJ.y*dtA.y-DEJ.y*dtD.y;
        D2JK.bz=-D2IJ.cy*d-D2JL.bz*(e+1.0)-DDJ.y*dtA.z-DEJ.y*dtD.z;
        D2JK.cx=-D2IJ.az*d-D2JL.cx*(e+1.0)-DDJ.z*dtA.x-DEJ.z*dtD.x;
        D2JK.cy=-D2IJ.bz*d-D2JL.cy*(e+1.0)-DDJ.z*dtA.y-DEJ.z*dtD.y;
        D2JK.cz=-D2IJ.cz*d-D2JL.cz*(e+1.0)-DDJ.z*dtA.z-DEJ.z*dtD.z;

        // Calculate the diagonal blocks of the hessian.
        if(index_i>=0)
        {
          HessianMatrix.element[index_i][index_i].re+=DDF*dtA.x*dtA.x+D2I.ax;
          HessianMatrix.element[index_i][index_i+1].re+=DDF*dtA.x*dtA.y+D2I.ay;
          HessianMatrix.element[index_i+1][index_i+1].re+=DDF*dtA.y*dtA.y+D2I.by;
          HessianMatrix.element[index_i][index_i+2].re+=DDF*dtA.x*dtA.z+D2I.az;
          HessianMatrix.element[index_i+1][index_i+2].re+=DDF*dtA.y*dtA.z+D2I.bz;
          HessianMatrix.element[index_i+2][index_i+2].re+=DDF*dtA.z*dtA.z+D2I.cz;
        }

        if(index_j>=0)
        {
          HessianMatrix.element[index_j][index_j].re+=DDF*dtB.x*dtB.x+D2J.ax;
          HessianMatrix.element[index_j][index_j+1].re+=DDF*dtB.x*dtB.y+D2J.ay;
          HessianMatrix.element[index_j+1][index_j+1].re+=DDF*dtB.y*dtB.y+D2J.by;
          HessianMatrix.element[index_j][index_j+2].re+=DDF*dtB.x*dtB.z+D2J.az;
          HessianMatrix.element[index_j+1][index_j+2].re+=DDF*dtB.y*dtB.z+D2J.bz;
          HessianMatrix.element[index_j+2][index_j+2].re+=DDF*dtB.z*dtB.z+D2J.cz;
        }

        if(index_k>=0)
        {
          HessianMatrix.element[index_k][index_k].re+=DDF*dtC.x*dtC.x+D2K.ax;
          HessianMatrix.element[index_k][index_k+1].re+=DDF*dtC.x*dtC.y+D2K.ay;
          HessianMatrix.element[index_k+1][index_k+1].re+=DDF*dtC.y*dtC.y+D2K.by;
          HessianMatrix.element[index_k][index_k+2].re+=DDF*dtC.x*dtC.z+D2K.az;
          HessianMatrix.element[index_k+1][index_k+2].re+=DDF*dtC.y*dtC.z+D2K.bz;
          HessianMatrix.element[index_k+2][index_k+2].re+=DDF*dtC.z*dtC.z+D2K.cz;
        }

        if(index_l>=0)
        {
          HessianMatrix.element[index_l][index_l].re+=DDF*dtD.x*dtD.x+D2L.ax;
          HessianMatrix.element[index_l][index_l+1].re+=DDF*dtD.x*dtD.y+D2L.ay;
          HessianMatrix.element[index_l+1][index_l+1].re+=DDF*dtD.y*dtD.y+D2L.by;
          HessianMatrix.element[index_l][index_l+2].re+=DDF*dtD.x*dtD.z+D2L.az;
          HessianMatrix.element[index_l+1][index_l+2].re+=DDF*dtD.y*dtD.z+D2L.bz;
          HessianMatrix.element[index_l+2][index_l+2].re+=DDF*dtD.z*dtD.z+D2L.cz;
        }

        if((index_i>=0)&&(index_j>=0))
        {
          if(index_i<index_j)
          {
            HessianMatrix.element[index_i][index_j].re+=DDF*dtA.x*dtB.x+D2IJ.ax;
            HessianMatrix.element[index_i][index_j+1].re+=DDF*dtA.x*dtB.y+D2IJ.ay;
            HessianMatrix.element[index_i][index_j+2].re+=DDF*dtA.x*dtB.z+D2IJ.az;
            HessianMatrix.element[index_i+1][index_j].re+=DDF*dtA.y*dtB.x+D2IJ.bx;
            HessianMatrix.element[index_i+1][index_j+1].re+=DDF*dtA.y*dtB.y+D2IJ.by;
            HessianMatrix.element[index_i+1][index_j+2].re+=DDF*dtA.y*dtB.z+D2IJ.bz;
            HessianMatrix.element[index_i+2][index_j].re+=DDF*dtA.z*dtB.x+D2IJ.cx;
            HessianMatrix.element[index_i+2][index_j+1].re+=DDF*dtA.z*dtB.y+D2IJ.cy;
            HessianMatrix.element[index_i+2][index_j+2].re+=DDF*dtA.z*dtB.z+D2IJ.cz;
          }
          else
          {
            HessianMatrix.element[index_j][index_i].re+=DDF*dtA.x*dtB.x+D2IJ.ax;
            HessianMatrix.element[index_j][index_i+1].re+=DDF*dtA.y*dtB.x+D2IJ.bx;
            HessianMatrix.element[index_j][index_i+2].re+=DDF*dtA.z*dtB.x+D2IJ.cx;
            HessianMatrix.element[index_j+1][index_i].re+=DDF*dtA.x*dtB.y+D2IJ.ay;
            HessianMatrix.element[index_j+1][index_i+1].re+=DDF*dtA.y*dtB.y+D2IJ.by;
            HessianMatrix.element[index_j+1][index_i+2].re+=DDF*dtA.z*dtB.y+D2IJ.cy;
            HessianMatrix.element[index_j+2][index_i].re+=DDF*dtA.x*dtB.z+D2IJ.az;
            HessianMatrix.element[index_j+2][index_i+1].re+=DDF*dtA.y*dtB.z+D2IJ.bz;
            HessianMatrix.element[index_j+2][index_i+2].re+=DDF*dtA.z*dtB.z+D2IJ.cz;
          }
        }

        if((index_i>=0)&&(index_k>=0))
        {
          if(index_i<index_k)
          {
            HessianMatrix.element[index_i][index_k].re+=DDF*dtA.x*dtC.x+D2IK.ax;
            HessianMatrix.element[index_i][index_k+1].re+=DDF*dtA.x*dtC.y+D2IK.ay;
            HessianMatrix.element[index_i][index_k+2].re+=DDF*dtA.x*dtC.z+D2IK.az;
            HessianMatrix.element[index_i+1][index_k].re+=DDF*dtA.y*dtC.x+D2IK.bx;
            HessianMatrix.element[index_i+1][index_k+1].re+=DDF*dtA.y*dtC.y+D2IK.by;
            HessianMatrix.element[index_i+1][index_k+2].re+=DDF*dtA.y*dtC.z+D2IK.bz;
            HessianMatrix.element[index_i+2][index_k].re+=DDF*dtA.z*dtC.x+D2IK.cx;
            HessianMatrix.element[index_i+2][index_k+1].re+=DDF*dtA.z*dtC.y+D2IK.cy;
            HessianMatrix.element[index_i+2][index_k+2].re+=DDF*dtA.z*dtC.z+D2IK.cz;
          }
          else
          {
            HessianMatrix.element[index_k][index_i].re+=DDF*dtA.x*dtC.x+D2IK.ax;
            HessianMatrix.element[index_k][index_i+1].re+=DDF*dtA.y*dtC.x+D2IK.bx;
            HessianMatrix.element[index_k][index_i+2].re+=DDF*dtA.z*dtC.x+D2IK.cx;
            HessianMatrix.element[index_k+1][index_i].re+=DDF*dtA.x*dtC.y+D2IK.ay;
            HessianMatrix.element[index_k+1][index_i+1].re+=DDF*dtA.y*dtC.y+D2IK.by;
            HessianMatrix.element[index_k+1][index_i+2].re+=DDF*dtA.z*dtC.y+D2IK.cy;
            HessianMatrix.element[index_k+2][index_i].re+=DDF*dtA.x*dtC.z+D2IK.az;
            HessianMatrix.element[index_k+2][index_i+1].re+=DDF*dtA.y*dtC.z+D2IK.bz;
            HessianMatrix.element[index_k+2][index_i+2].re+=DDF*dtA.z*dtC.z+D2IK.cz;
          }
        }

        if((index_i>=0)&&(index_l>=0))
        {
          if(index_i<index_l)
          {
            HessianMatrix.element[index_i][index_l].re+=DDF*dtA.x*dtD.x+D2IL.ax;
            HessianMatrix.element[index_i][index_l+1].re+=DDF*dtA.x*dtD.y+D2IL.ay; 
            HessianMatrix.element[index_i][index_l+2].re+=DDF*dtA.x*dtD.z+D2IL.az; 
            HessianMatrix.element[index_i+1][index_l].re+=DDF*dtA.y*dtD.x+D2IL.bx; 
            HessianMatrix.element[index_i+1][index_l+1].re+=DDF*dtA.y*dtD.y+D2IL.by;
            HessianMatrix.element[index_i+1][index_l+2].re+=DDF*dtA.y*dtD.z+D2IL.bz;
            HessianMatrix.element[index_i+2][index_l].re+=DDF*dtA.z*dtD.x+D2IL.cx;
            HessianMatrix.element[index_i+2][index_l+1].re+=DDF*dtA.z*dtD.y+D2IL.cy;
            HessianMatrix.element[index_i+2][index_l+2].re+=DDF*dtA.z*dtD.z+D2IL.cz;
          }
          else
          {
            HessianMatrix.element[index_l][index_i].re+=DDF*dtA.x*dtD.x+D2IL.ax;
            HessianMatrix.element[index_l][index_i+1].re+=DDF*dtA.y*dtD.x+D2IL.bx;
            HessianMatrix.element[index_l][index_i+2].re+=DDF*dtA.z*dtD.x+D2IL.cx;
            HessianMatrix.element[index_l+1][index_i].re+=DDF*dtA.x*dtD.y+D2IL.ay;
            HessianMatrix.element[index_l+1][index_i+1].re+=DDF*dtA.y*dtD.y+D2IL.by;
            HessianMatrix.element[index_l+1][index_i+2].re+=DDF*dtA.z*dtD.y+D2IL.cy;
            HessianMatrix.element[index_l+2][index_i].re+=DDF*dtA.x*dtD.z+D2IL.az;
            HessianMatrix.element[index_l+2][index_i+1].re+=DDF*dtA.y*dtD.z+D2IL.bz;
            HessianMatrix.element[index_l+2][index_i+2].re+=DDF*dtA.z*dtD.z+D2IL.cz;
          }
        }

        if((index_j>=0)&&(index_k>=0))
        {
          if(index_j<index_k)
          {
            HessianMatrix.element[index_j][index_k].re+=DDF*dtB.x*dtC.x+D2JK.ax;
            HessianMatrix.element[index_j][index_k+1].re+=DDF*dtB.x*dtC.y+D2JK.ay; 
            HessianMatrix.element[index_j][index_k+2].re+=DDF*dtB.x*dtC.z+D2JK.az;
            HessianMatrix.element[index_j+1][index_k].re+=DDF*dtB.y*dtC.x+D2JK.bx;
            HessianMatrix.element[index_j+1][index_k+1].re+=DDF*dtB.y*dtC.y+D2JK.by;
            HessianMatrix.element[index_j+1][index_k+2].re+=DDF*dtB.y*dtC.z+D2JK.bz;
            HessianMatrix.element[index_j+2][index_k].re+=DDF*dtB.z*dtC.x+D2JK.cx;
            HessianMatrix.element[index_j+2][index_k+1].re+=DDF*dtB.z*dtC.y+D2JK.cy;
            HessianMatrix.element[index_j+2][index_k+2].re+=DDF*dtB.z*dtC.z+D2JK.cz;
          }
          else
          {
            HessianMatrix.element[index_k][index_j].re+=DDF*dtB.x*dtC.x+D2JK.ax;
            HessianMatrix.element[index_k][index_j+1].re+=DDF*dtB.y*dtC.x+D2JK.bx;
            HessianMatrix.element[index_k][index_j+2].re+=DDF*dtB.z*dtC.x+D2JK.cx;
            HessianMatrix.element[index_k+1][index_j].re+=DDF*dtB.x*dtC.y+D2JK.ay;
            HessianMatrix.element[index_k+1][index_j+1].re+=DDF*dtB.y*dtC.y+D2JK.by;
            HessianMatrix.element[index_k+1][index_j+2].re+=DDF*dtB.z*dtC.y+D2JK.cy;
            HessianMatrix.element[index_k+2][index_j].re+=DDF*dtB.x*dtC.z+D2JK.az;
            HessianMatrix.element[index_k+2][index_j+1].re+=DDF*dtB.y*dtC.z+D2JK.bz;
            HessianMatrix.element[index_k+2][index_j+2].re+=DDF*dtB.z*dtC.z+D2JK.cz;
          }
        }

        if((index_j>=0)&&(index_l>=0))
        {
          if(index_j<index_l)
          {
            HessianMatrix.element[index_j][index_l].re+=DDF*dtB.x*dtD.x+D2JL.ax;
            HessianMatrix.element[index_j][index_l+1].re+=DDF*dtB.x*dtD.y+D2JL.ay;
            HessianMatrix.element[index_j][index_l+2].re+=DDF*dtB.x*dtD.z+D2JL.az;
            HessianMatrix.element[index_j+1][index_l].re+=DDF*dtB.y*dtD.x+D2JL.bx;
            HessianMatrix.element[index_j+1][index_l+1].re+=DDF*dtB.y*dtD.y+D2JL.by;
            HessianMatrix.element[index_j+1][index_l+2].re+=DDF*dtB.y*dtD.z+D2JL.bz;
            HessianMatrix.element[index_j+2][index_l].re+=DDF*dtB.z*dtD.x+D2JL.cx;
            HessianMatrix.element[index_j+2][index_l+1].re+=DDF*dtB.z*dtD.y+D2JL.cy;
            HessianMatrix.element[index_j+2][index_l+2].re+=DDF*dtB.z*dtD.z+D2JL.cz;
          }
          else
          {
            HessianMatrix.element[index_l][index_j].re+=DDF*dtB.x*dtD.x+D2JL.ax;
            HessianMatrix.element[index_l][index_j+1].re+=DDF*dtB.y*dtD.x+D2JL.bx;
            HessianMatrix.element[index_l][index_j+2].re+=DDF*dtB.z*dtD.x+D2JL.cx;
            HessianMatrix.element[index_l+1][index_j].re+=DDF*dtB.x*dtD.y+D2JL.ay;
            HessianMatrix.element[index_l+1][index_j+1].re+=DDF*dtB.y*dtD.y+D2JL.by;
            HessianMatrix.element[index_l+1][index_j+2].re+=DDF*dtB.z*dtD.y+D2JL.cy;
            HessianMatrix.element[index_l+2][index_j].re+=DDF*dtB.x*dtD.z+D2JL.az;
            HessianMatrix.element[index_l+2][index_j+1].re+=DDF*dtB.y*dtD.z+D2JL.bz;
            HessianMatrix.element[index_l+2][index_j+2].re+=DDF*dtB.z*dtD.z+D2JL.cz;
          }
        }

        if((index_k>=0)&&(index_l>=0))
        {
          if(index_k<index_l)
          {
            HessianMatrix.element[index_k][index_l].re+=DDF*dtC.x*dtD.x+D2KL.ax;
            HessianMatrix.element[index_k][index_l+1].re+=DDF*dtC.x*dtD.y+D2KL.ay;
            HessianMatrix.element[index_k][index_l+2].re+=DDF*dtC.x*dtD.z+D2KL.az;
            HessianMatrix.element[index_k+1][index_l].re+=DDF*dtC.y*dtD.x+D2KL.bx;
            HessianMatrix.element[index_k+1][index_l+1].re+=DDF*dtC.y*dtD.y+D2KL.by;
            HessianMatrix.element[index_k+1][index_l+2].re+=DDF*dtC.y*dtD.z+D2KL.bz;
            HessianMatrix.element[index_k+2][index_l].re+=DDF*dtC.z*dtD.x+D2KL.cx;
            HessianMatrix.element[index_k+2][index_l+1].re+=DDF*dtC.z*dtD.y+D2KL.cy;
            HessianMatrix.element[index_k+2][index_l+2].re+=DDF*dtC.z*dtD.z+D2KL.cz;
          }
          else
          {
            HessianMatrix.element[index_l][index_k].re+=DDF*dtC.x*dtD.x+D2KL.ax;
            HessianMatrix.element[index_l][index_k+1].re+=DDF*dtC.y*dtD.x+D2KL.bx;
            HessianMatrix.element[index_l][index_k+2].re+=DDF*dtC.z*dtD.x+D2KL.cx;
            HessianMatrix.element[index_l+1][index_k].re+=DDF*dtC.x*dtD.y+D2KL.ay;
            HessianMatrix.element[index_l+1][index_k+1].re+=DDF*dtC.y*dtD.y+D2KL.by;
            HessianMatrix.element[index_l+1][index_k+2].re+=DDF*dtC.z*dtD.y+D2KL.cy;
            HessianMatrix.element[index_l+2][index_k].re+=DDF*dtC.x*dtD.z+D2KL.az;
            HessianMatrix.element[index_l+2][index_k+1].re+=DDF*dtC.y*dtD.z+D2KL.bz;
            HessianMatrix.element[index_l+2][index_k+2].re+=DDF*dtC.z*dtD.z+D2KL.cz;
          }
        }
      }
    }
  }
}