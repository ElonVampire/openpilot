#include "acado_common.h"
#include "acado_auxiliary_functions.h"

#include <stdio.h>

#define NX          ACADO_NX  /* Number of differential state variables.  */
#define NXA         ACADO_NXA /* Number of algebraic variables. */
#define NU          ACADO_NU  /* Number of control inputs. */
#define NOD         ACADO_NOD  /* Number of online data values. */

#define NY          ACADO_NY  /* Number of measurements/references on nodes 0..N - 1. */
#define NYN         ACADO_NYN /* Number of measurements/references on node N. */

#define N           ACADO_N   /* Number of intervals in the horizon. */

ACADOvariables acadoVariables;
ACADOworkspace acadoWorkspace;

typedef struct {
  double x, y, psi, delta, rate;
} state_t;

const int N_steps = 15;
double T_IDXS[N_steps + 2] = {0.0, 0.00976562, 0.0390625 , 0.08789062, 0.15625,
                     0.24414062, 0.3515625 , 0.47851562, 0.625     , 0.79101562,
                     0.9765625 , 1.18164062, 1.40625   , 1.65039062, 1.9140625 ,
                     2.19726562, 2.5};


typedef struct {
  double x[N+1];
  double y[N+1];
  double psi[N+1];
  double delta[N+1];
  double rate[N];
  double cost;
} log_t;

void set_weights(double pathCost, double headingCost, double yawRateCost, double steerRateCost){
  int    i;
  for (i = 0; i < N; i++) {
    double mult = 20* (T_IDXS[i+1] - T_IDXS[i]);
    // Setup diagonal entries
    acadoVariables.W[NY*NY*i + (NY+1)*0] = mult * pathCost;
    acadoVariables.W[NY*NY*i + (NY+1)*1] = mult * headingCost;
    acadoVariables.W[NY*NY*i + (NY+1)*2] = mult * yawRateCost;
    acadoVariables.W[NY*NY*i + (NY+1)*3] = mult * steerRateCost;
  }
  acadoVariables.WN[(NYN+1)*0] = 3*pathCost;
  acadoVariables.WN[(NYN+1)*1] = 3*headingCost;
}

void init(double pathCost, double headingCost, double yawRateCost, double steerRateCost){
  acado_initializeSolver();
  int    i;

  /* Initialize the states and controls. */
  for (i = 0; i < NX * (N + 1); ++i)  acadoVariables.x[ i ] = 0.0;
  for (i = 0; i < NU * N; ++i)  acadoVariables.u[ i ] = 0.0 ;

  /* Initialize the measurements/reference. */
  for (i = 0; i < NY * N; ++i)  acadoVariables.y[ i ] = 0.0;
  for (i = 0; i < NYN; ++i)  acadoVariables.yN[ i ] = 0.0;

  /* MPC: initialize the current state feedback. */
  for (i = 0; i < NX; ++i) acadoVariables.x0[ i ] = 0.0;

  set_weights(pathCost, headingCost, yawRateCost, steerRateCost);
}

int run_mpc(state_t * x0, log_t * solution, double v_ego,
             double target_y[N+1], double target_psi[N+1],
             double target_dpsi[N+1], double curvature_factor){

  int    i;

  for (i = 0; i <= NOD * N; i+= NOD){
    acadoVariables.od[i+0] = v_ego;
    acadoVariables.od[i+1] = curvature_factor;
  }
  for (i = 0; i < N; i+= 1){
    acadoVariables.y[NY*i + 0] = target_y[i];
    acadoVariables.y[NY*i + 1] = (v_ego + 1) * target_psi[i];
    acadoVariables.y[NY*i + 2] = target_dpsi[i];
    acadoVariables.y[NY*i + 3] = 0.0;
  }
  acadoVariables.yN[0] = target_y[N];
  acadoVariables.yN[1] = (v_ego + 1) * target_psi[N];

  acadoVariables.x0[0] = x0->x;
  acadoVariables.x0[1] = x0->y;
  acadoVariables.x0[2] = x0->psi;
  acadoVariables.x0[3] = x0->delta;


  acado_preparationStep();
  acado_feedbackStep();

  /* printf("lat its: %d\n", acado_getNWSR());  // n iterations
  printf("Objective: %.6f\n", acado_getObjective());  // solution cost */

  for (i = 0; i <= N; i++){
    solution->x[i] = acadoVariables.x[i*NX];
    solution->y[i] = acadoVariables.x[i*NX+1];
    solution->psi[i] = acadoVariables.x[i*NX+2];
    solution->delta[i] = acadoVariables.x[i*NX+3];
    if (i < N){
      solution->rate[i] = acadoVariables.u[i];
    }
  }
  solution->cost = acado_getObjective();

  // Dont shift states here. Current solution is closer to next timestep than if
  // we use the old solution as a starting point
  //acado_shiftStates(2, 0, 0);
  //acado_shiftControls( 0 );

  return acado_getNWSR();
}
