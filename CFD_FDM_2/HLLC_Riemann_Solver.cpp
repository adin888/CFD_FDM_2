#include"cfd_head.h"

/*
* -Using HLLC scheme to approximate Riemann solver
* -Using WENO-5 Scheme to reconstruct the left and right side fluxes at the interface
* -Using Runge-Kutta-3 Scheme for time integration
* -Data is saved every 0.02s
*/

/*
* Calculate right hand term of the inviscid Burgers equation
* r = -udu/dx
*/
vector< vector<double> > HRrhs(int nx, double dx, double gamma, vector< vector<double> > q, vector< vector<double> > r)
{
    vector< vector<double> > qL(nx + 1, vector<double>(3));          //left and right states of field at discret nodal points
    vector< vector<double> > qR(nx + 1, vector<double>(3));
    vector< vector<double> > fL(nx + 1, vector<double>(3));          //left and right states of fluxes at the interface   
    vector< vector<double> > fR(nx + 1, vector<double>(3));

    vector< vector<double> > f(nx + 1, vector<double>(3));           //flux at the interface computed using Rusanov scheme


    qL = RRweno5L(nx, q, qL);
    qR = RRweno5R(nx, q, qR);

    fL = fluxes(nx, gamma, qL, fL);
    fR = fluxes(nx, gamma, qR, fR);

    //f = roe(nx, gamma, qL, qR, f, fL, fR);
    f = HLLC(nx, gamma, qL, qR, f, fL, fR);


    for (int i = 0; i < nx; i++)
    {
        for (int m = 0; m < 3; m++)
        {
            r[i][m] = -(f[i + 1][m] - f[i][m]) / dx;
        }

    }

    return r;
}

vector< vector<double> > HLLC(int nx, double gamma, vector< vector<double> > qL, vector< vector<double> > qR,
    vector< vector<double> > f, vector< vector<double> > fL, vector< vector<double> > fR)
{
    vector<double> Ds(3);                              
    double rL, uL, eL, pL, aL, rR, uR, eR, pR, aR;
    double SL, SR, SP, PLR;                                                  
    double gm = gamma - 1.0;

    Ds[0] = 0.0;
    Ds[1] = 1.0;

    for (int i = 0; i < nx + 1; i++)
    {
        /* Compute Left and right states of rho(r), u, p, h, e */
        rL = qL[i][0];
        uL = qL[i][1] / rL;
        eL = qL[i][2] / rL;
        pL = gm * (eL * rL - 0.5 * rL * (uL * uL));
        aL = sqrt(abs(gamma * pL / rL));

        rR = qR[i][0];
        uR = qR[i][1] / rR;
        eR = qR[i][2] / rR;
        pR = gm * (eR * rR - 0.5 * rR * (uR * uR));
        aR = sqrt(abs(gamma * pR / rR));

        /* Lower and upper bound for the left and right state characteristics speed */
        SL = min(uL, uR) - max(aL, aR);
        SR = max(uL, uR) + max(aL, aR);

        /* middle wave of speed */
        SP = (pR - pL + rL * uL * (SL - uL) - rR * uR * (SR - uR)) / (rL * (SL - uL) - rR * (SR - uR));
        Ds[2] = SP;

        /* The mean pressure */
        PLR = 0.5 * (pL + pR + rL * (SL - uL) * (SP - uL) + rR * (SR - uR) * (SP - uR));

        /* compute the fluxes using HLLC scheme */
        if (SL >= 0.0)
        {
            for (int m = 0; m < 3; m++)
            {
                f[i][m] = fL[i][m];
            }
        }
        else if (SR <= 0.0)
        {
            for (int m = 0; m < 3; m++)
            {
                f[i][m] = fR[i][m];
            }
        }
        else if ((SP >= 0.0) && (SL <= 0.0))
        {
            for (int m = 0; m < 3; m++)
            {
                f[i][m] = (SP * (SL * qL[i][m] - fL[i][m]) + SL * PLR * Ds[m]) / (SL - SP);
            }
        }
        else if ((SP <= 0.0) && (SR >= 0.0))
        {
            for (int m = 0; m < 3; m++)
            {
                f[i][m] = (SP * (SR * qR[i][m] - fR[i][m]) + SR * PLR * Ds[m]) / (SR - SP);
            }
        }

    }
    return f;
}
vector < vector< vector<double> > > Numerical_HLLC(int nx, int ns, int nt, double dx, double dt, vector<double> x, vector < vector< vector<double> > > q)
{
    vector< vector<double> > qn(nx, vector<double>(3));
    vector< vector<double> > qt(nx, vector<double>(3));
    vector< vector<double> > r(nx, vector<double>(3));
    double rho, u, p, e;

    int freq = ceil(nt / ns);

    double gamma = 1.4;            //specific gas ratio

    /* Sod's Riemann problem */
    double rhoL = 1.0;
    double uL = 0.0;
    double pL = 1.0;

    double rhoR = 0.125;
    double uR = 0.0;
    double pR = 0.1;

    double xc = 0.5;               //seperator location

    for (int i = 0; i < nx; i++)
    {
        if (x[i] > xc)
        {
            rho = rhoR;
            u = uR;
            p = pR;
        }
        else
        {
            rho = rhoL;
            u = uL;
            p = pL;
        }
        e = p / (rho * (gamma - 1.0)) + 0.5 * u * u;

        qn[i][0] = rho;
        qn[i][1] = rho * u;
        qn[i][2] = rho * e;
    }

    for (int i = 0; i < nx; i++)
    {
        for (int m = 0; m < 3; m++)
        {
            q[i][m][0] = qn[i][m];
        }
    }

    for (int j = 1; j < nt + 1; j++)
    {
        r = HRrhs(nx, dx, gamma, qn, r);
        for (int i = 0; i < nx; i++)
        {
            for (int m = 0; m < 3; m++)
            {
                qt[i][m] = qn[i][m] + dt * r[i][m];
            }
        }

        r = HRrhs(nx, dx, gamma, qt, r);
        for (int i = 0; i < nx; i++)
        {
            for (int m = 0; m < 3; m++)
            {
                qt[i][m] = 0.75 * qn[i][m] + 0.25 * qt[i][m] + 0.25 * dt * r[i][m];
            }
        }

        r = HRrhs(nx, dx, gamma, qt, r);
        for (int i = 0; i < nx; i++)
        {
            for (int m = 0; m < 3; m++)
            {
                qn[i][m] = (1.0 / 3.0) * qn[i][m] + (2.0 / 3.0) * qt[i][m] + (2.0 / 3.0) * dt * r[i][m];
            }
        }

        if (j % freq == 0)
        {
            for (int i = 0; i < nx; i++)
            {
                for (int m = 0; m < 3; m++)
                {
                    q[i][m][j / freq] = qn[i][m];
                }
            }
        }

    }
    return q;
}

void HLLC_Riemann_Solver()
{
    double x_l = 0.0;
    double x_r = 1.0;
    int nx = 256;
    double dx = (x_r - x_l) / nx;

    double t = 0.2;
    double dt = 0.0001;
    int nt = ceil(t / dt);

    int ns = 10;   //Save ten sets of data results
    double ds = t / ns;          //The time interval for saving data

    vector<double> x(nx);
    vector < vector< vector<double> > > q(nx, vector<vector<double>>(3, vector<double>(ns + 1)));

    for (int i = 0; i < nx; i++)
    {
        x[i] = x_l + dx * i + 0.5 * dx;  //Assign node locations
    }

    q = Numerical_HLLC(nx, ns, nt, dx, dt, x, q);

    ofstream rho;
    ofstream u;
    ofstream e;
    rho.open("rho_final.csv", ios::out | ios::trunc);
    u.open("u_final.csv", ios::out | ios::trunc);
    e.open("e_final.csv", ios::out | ios::trunc);

    rho << "x" << ",";
    u << "x" << ",";
    e << "x" << ",";
    for (int i = 0; i < ns + 1; i++)
    {
        rho << i * ds << ",";
        u << i * ds << ",";
        e << i * ds << ",";
    }
    rho << endl;
    u << endl;
    e << endl;

    for (int i = 0; i < nx; i++)
    {
        rho << setprecision(16) << x[i] << ",";
        u << setprecision(16) << x[i] << ",";
        e << setprecision(16) << x[i] << ",";
        for (int j = 0; j < ns + 1; j++)
        {
            rho << setprecision(16) << q[i][0][j] << ",";
            u << setprecision(16) << q[i][1][j] / q[i][0][j] << ",";
            e << setprecision(16) << q[i][2][j] / q[i][0][j] << ",";
        }
        rho << endl;
        u << endl;
        e << endl;
    }

    rho.close();
    u.close();
    e.close();
}