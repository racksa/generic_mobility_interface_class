// rigid_body.cpp

#include <cmath>
#include <string>
#include <algorithm>
#include "rigid_body.hpp"
#include "seeding.hpp"

rigid_body::~rigid_body(){}

rigid_body::rigid_body(){}

void rigid_body::initial_setup(const int id, double *const f_address, const double *const data_from_file){

  blob_references = std::vector<double>(3*NBLOB);

  Q_init = matrix(3,3);

  #if (USE_BROYDEN_FOR_EVERYTHING && !INFINITE_PLANE_WALL)

    blob_forces = f_address;

    blob_forces_m1 = std::vector<double>(3*NBLOB);
    blob_forces_m2 = std::vector<double>(3*NBLOB);

  #endif

  #if READ_INITIAL_CONDITIONS_FROM_BACKUP

    int p = 0;

    q.scalar_part = data_from_file[p++];
    q.vector_part[0] = data_from_file[p++];
    q.vector_part[1] = data_from_file[p++];
    q.vector_part[2] = data_from_file[p++];

    qm1 = q;

    x[0] = data_from_file[p++];
    x[1] = data_from_file[p++];
    x[2] = data_from_file[p++];
    xm1[0] = data_from_file[p++];
    xm1[1] = data_from_file[p++];
    xm1[2] = data_from_file[p++];
    xm2[0] = data_from_file[p++];
    xm2[1] = data_from_file[p++];
    xm2[2] = data_from_file[p++];

    u[0] = data_from_file[p++];
    u[1] = data_from_file[p++];
    u[2] = data_from_file[p++];
    um1[0] = data_from_file[p++];
    um1[1] = data_from_file[p++];
    um1[2] = data_from_file[p++];

    #if USE_BROYDEN_FOR_EVERYTHING

      // Do this last so we can start GMRES sims from broyden-only backups.
      for (int n = 0; n < 3*NBLOB; n++){

        blob_forces[n] = data_from_file[p++];
        blob_forces_m1[n] = data_from_file[p++];
        blob_forces_m2[n] = data_from_file[p++];

      }

    #endif

  #endif

  #if INFINITE_PLANE_WALL

    #if !READ_INITIAL_CONDITIONS_FROM_BACKUP

      x[0] = 0.0;
      x[1] = 0.0;
      x[2] = 0.0;
      xm1[0] = x[0];
      xm1[1] = x[1];
      xm1[2] = x[2];
      xm2[0] = x[0];
      xm2[1] = x[1];
      xm2[2] = x[2];

      u[0] = 0.0;
      u[1] = 0.0;
      u[2] = 0.0;
      um1[0] = 0.0;
      um1[1] = 0.0;
      um1[2] = 0.0;

      q = quaternion(1.0, 0.0, 0.0, 0.0);
      qm1 = q;

    #endif

  #elif SADDLE_BODIES

    // Setting both to INFINITY will form a plane.
    const double radius_of_curvature_x = INFINITY;
    const double radius_of_curvature_y = INFINITY;

    const double grid_length_x = DL*NSEG;
    const double grid_length_y = DL*NSEG;
    const int surf_grid_dim_x = round(sqrt(grid_length_x*NBLOB/grid_length_y));
    const int surf_grid_dim_y = ceil(NBLOB/double(surf_grid_dim_x));
    const double surf_grid_step_x = (surf_grid_dim_x > 1) ? grid_length_x/(double(surf_grid_dim_x) - 1.0) : 0.0;
    const double surf_grid_step_y = (surf_grid_dim_y > 1) ? grid_length_y/(double(surf_grid_dim_y) - 1.0) : 0.0;

    #if !READ_INITIAL_CONDITIONS_FROM_BACKUP

      const double body_spacing = std::max<double>(grid_length_x, grid_length_y) + 3.0*DL*NSEG;

      x[0] = id*body_spacing;
      x[1] = 0.0;
      x[2] = 0.0;
      xm1[0] = x[0];
      xm1[1] = x[1];
      xm1[2] = x[2];
      xm2[0] = x[0];
      xm2[1] = x[1];
      xm2[2] = x[2];

      u[0] = 0.0;
      u[1] = 0.0;
      u[2] = 0.0;
      um1[0] = 0.0;
      um1[1] = 0.0;
      um1[2] = 0.0;

      q = quaternion(1.0, 0.0, 0.0, 0.0);
      qm1 = q;

    #endif

    const double im = 0.5*(surf_grid_dim_x - 1.0);
    const double jm = 0.5*(surf_grid_dim_y - 1.0);

    const double theta_x = surf_grid_step_x/radius_of_curvature_x;
    const double theta_y = surf_grid_step_y/radius_of_curvature_y;

    // The multiblob method won't work if the point we designate as the position of the body
    // coincides with one of the blob positions, so we displace it from the body.
    const double height_offset = RBLOB;

    for (int i = 0; i < surf_grid_dim_x; i++){
      for (int j = 0; j < surf_grid_dim_y; j++){

        const int id = 3*(j + i*surf_grid_dim_y);

        if (blob_id < NBLOB){

          if (std::isinf(radius_of_curvature_x) && std::isinf(radius_of_curvature_y)){

            blob_references[id] = (i-im)*surf_grid_step_x;
            blob_references[id + 1] = (j-jm)*surf_grid_step_y;
            blob_references[id + 2] = height_offset;

          } else if (std::isinf(radius_of_curvature_x)) {

            blob_references[id] = (i-im)*surf_grid_step_x;
            blob_references[id + 1] = radius_of_curvature_y*sin(theta_y*(j - jm));
            blob_references[id + 2] = height_offset + radius_of_curvature_y*(1.0 - cos(theta_y*(j - jm)));

          } else if (std::isinf(radius_of_curvature_y)) {

            blob_references[id] = radius_of_curvature_x*sin(theta_x*(i - im));
            blob_references[id + 1] = (j-jm)*surf_grid_step_y;
            blob_references[id + 2] = height_offset + radius_of_curvature_x*(1.0 - cos(theta_x*(i - im)));

          } else {

            blob_references[id] = radius_of_curvature_x*sin(theta_x*(i - im));
            blob_references[id + 1] = radius_of_curvature_y*sin(theta_y*(j - jm));
            blob_references[id + 2] = height_offset + radius_of_curvature_x*(1.0 - cos(theta_x*(i - im))) + radius_of_curvature_y*(1.0 - cos(theta_y*(j - jm)));

          }

        }

      }
    }

  #elif SPHEROID_BODIES

    const double Rsphere = double(R_OVER_L)*DL*NSEG;

    #if !READ_INITIAL_CONDITIONS_FROM_BACKUP

      const double body_spacing = 3.0*(Rsphere + DL*NSEG);

      x[0] = id*body_spacing;
      x[1] = 0.0;
      x[2] = 0.0;
      xm1[0] = x[0];
      xm1[1] = x[1];
      xm1[2] = x[2];
      xm2[0] = x[0];
      xm2[1] = x[1];
      xm2[2] = x[2];

      u[0] = 0.0;
      u[1] = 0.0;
      u[2] = 0.0;
      um1[0] = 0.0;
      um1[1] = 0.0;
      um1[2] = 0.0;

      q = quaternion(1.0, 0.0, 0.0, 0.0);
      qm1 = q;

    #endif

    std::ifstream input_file("sphere"+std::to_string(NBLOB)+".seed");

    if (input_file.good()){

      for (int i = 0; i < 3*NBLOB; i++){

        input_file >> blob_references[i];

      }

    } else {

      repulsive_blob_seeding(&blob_references[0]);

    }

    // The seeding functions work on unit spheres, so the scaling must be done after we read or calculate.
    for (int i = 0; i < 3*NBLOB; i++){

      blob_references[i] *= Rsphere;

    }

    input_file.close();

  #elif TORUS_BODIES

    // NOT YET IMPLEMENTED

  #endif

}

void rigid_body::initial_guess(const int nt){

  double initial_guess_x[3], initial_guess_u[3];

  if (nt <= NUM_EULER_STEPS){

    initial_guess_x[0] = 2.0*x[0] - xm1[0];
    initial_guess_x[1] = 2.0*x[1] - xm1[1];
    initial_guess_x[2] = 2.0*x[2] - xm1[2];

    initial_guess_u[0] = u[0];
    initial_guess_u[1] = u[1];
    initial_guess_u[2] = u[2];

  } else {

    initial_guess_x[0] = 3.0*(x[0] - xm1[0]) + xm2[0];
    initial_guess_x[1] = 3.0*(x[1] - xm1[1]) + xm2[1];
    initial_guess_x[2] = 3.0*(x[2] - xm1[2]) + xm2[2];

    initial_guess_u[0] = 2.0*u[0] - um1[0];
    initial_guess_u[1] = 2.0*u[1] - um1[1];
    initial_guess_u[2] = 2.0*u[2] - um1[2];

  }

  xm2[0] = xm1[0];
  xm2[1] = xm1[1];
  xm2[2] = xm1[2];
  xm1[0] = x[0];
  xm1[1] = x[1];
  xm1[2] = x[2];
  x[0] = initial_guess_x[0];
  x[1] = initial_guess_x[1];
  x[2] = initial_guess_x[2];

  um1[0] = u[0];
  um1[1] = u[1];
  um1[2] = u[2];
  u[0] = initial_guess_u[0];
  u[1] = initial_guess_u[1];
  u[2] = initial_guess_u[2];

  qm1 = q;
  lie_exp(q, u);
  q *= qm1;

  q.rot_mat(Q_init);

  #if USE_BROYDEN_FOR_EVERYTHING

    if (nt == 1){

      for (int n = 0; n < 3*NBLOB; n++){

        blob_forces_m1[n] = blob_forces[n];

      }

    } else if (nt == 2){

      for (int n = 0; n < 3*NBLOB; n++){

        blob_forces_m2[n] = blob_forces_m1[n];
        blob_forces_m1[n] = blob_forces[n];
        blob_forces[n] = 2.0*blob_forces_m1[n] - blob_forces_m2[n];

      }

    } else if (nt > 2){

      for (int n = 0; n < 3*NBLOB; n++){

        const double temp = 3.0*(blob_forces[n] - blob_forces_m1[n]) + blob_forces_m2[n];

        blob_forces_m2[n] = blob_forces_m1[n];
        blob_forces_m1[n] = blob_forces[n];
        blob_forces[n] = temp;

      }

    }

  #endif

}

void rigid_body::blob_positions(double *const x_array) const {

  const matrix R = q.rot_mat();

  const matrix X(3, 1, x);

  for (int i = 0; i < NBLOB; i++){

    const matrix ref(3, 1, &blob_references[3*i]);

    const matrix pos = X + R*ref;

    x_array[3*i] = pos(0);
    x_array[3*i + 1] = pos(1);
    x_array[3*i + 2] = pos(2);

  }

}

void rigid_body::update(const double *const body_update){

  #if !INFINITE_PLANE_WALL

    #if USE_BROYDEN_FOR_EVERYTHING

      for (int n = 0; n < 3*NBLOB; n++){

        blob_forces[n] += body_update[6 + n];

      }

    #endif

    x[0] += body_update[0];
    x[1] += body_update[1];
    x[2] += body_update[2];

    u[0] += body_update[3];
    u[1] += body_update[4];
    u[2] += body_update[5];

    lie_exp(q, u);
    q *= qm1;

  #endif

}

void rigid_body::write_reference_positions() const {

  #if !INFINITE_PLANE_WALL

    std::ofstream blob_ref_file(SIMULATION_NAME+std::string("_blob_references.dat"));

    for (int n = 0; n < 3*NBLOB; n++){

      blob_ref_file << blob_references[n] << " " ;

    }

    blob_ref_file << "\n";
    blob_ref_file.close();

  #endif

}

void rigid_body::write_data(std::ofstream& body_state_file) const {

  body_state_file << x[0] << " " << x[1] << " " << x[2] << " " << q.scalar_part << " " << q.vector_part[0] << " " << q.vector_part[1] << " " << q.vector_part[2] << " ";

}

void rigid_body::write_backup(std::ofstream& backup_file) const {

  backup_file << q.scalar_part << " " << q.vector_part[0] << " " << q.vector_part[1] << " " << q.vector_part[2] << " ";

  backup_file << x[0] << " " << x[1] << " " << x[2] << " ";
  backup_file << xm1[0] << " " << xm1[1] << " " << xm1[2] << " ";
  backup_file << xm2[0] << " " << xm2[1] << " " << xm2[2] << " ";

  backup_file << u[0] << " " << u[1] << " " << u[2] << " ";
  backup_file << um1[0] << " " << um1[1] << " " << um1[2] << " ";

  #if USE_BROYDEN_FOR_EVERYTHING

    // Do this after everything else so we can start GMRES from Broyden-only backups.
    for (int n = 0; n < 3*NBLOB; n++){

      backup_file << blob_forces[n] << " ";
      backup_file << blob_forces_m1[n] << " ";
      backup_file << blob_forces_m2[n] << " ";

    }

  #endif

}

#if PRESCRIBED_BODY_VELOCITIES

  void rigid_body::prescribed_translational_velocity(double *const V, const double t, const int id) const {

    V[0] = 0.0;
    V[1] = 0.0;
    V[2] = 0.0;

  }

  void rigid_body::prescribed_rotational_velocity(double *const W, const double t, const int id) const {

    W[0] = 0.0;
    W[1] = 0.0;
    W[2] = 0.0;

  }

#endif
