
/* STATEMENT:

GEO_TOP MODELS THE ENERGY AND WATER FLUXES AT LAND SURFACE
GEOtop-Version 0.9375-Subversion Mackenzie

Copyright, 2008 Stefano Endrizzi, Riccardo Rigon, Emanuele Cordano, Matteo Dall'Amico

 LICENSE:

 This file is part of GEOtop 0.9375 Mackenzie.
 GEOtop is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.*/


/*Authors: Stefano Endrizzi and Matteo Dall'Amico - October 2008*/
#include "keywords_file.h"
#include "constant.h"
#include "struct.geotop.09375.h"
#include "water.balance_1D.h"
#include "pedo.funct.h"
#include "util_math.h"

extern T_INIT *UV;
extern STRINGBIN *files;
extern char *error_file_name;
extern long Nl, Nr, Nc;
extern double NoV;

/*--------------------------------------------*/
void water_balance_1D(TOPO *top, SOIL *sl, LAND *land, WATER *wat, CHANNEL *cnet, PAR *par, double time){

	double Dt;
	long i;

	for(i=1;i<=par->nDt_water;i++){

		initialize_doublevector(cnet->Qsup_spread,0.0);
		initialize_doublevector(cnet->Qsub_spread,0.0);

		Dt=par->Dt/(double)par->nDt_water;

		subflow(Dt, land, top, sl, par, wat);

		vertical_water_balance(Dt, land->LC, sl, wat, time, par);

		supflow(Dt, Dt, top, land, wat, cnet, par);

		routing(cnet);

		if(par->state_pixel==1) output_waterbalance(Dt, wat, sl, par, land->LC);

	}

}


/*--------------------------------------------*/

void vertical_water_balance(double Dt, DOUBLEMATRIX *Z, SOIL *sl, WATER *wat, double time, PAR *par)

{

	long l;					/* the index of depth is l not d because d is used for sl-thickness vector*/
	long r,c;				/* counters of rows and columns*/
	long i;
	DOUBLEVECTOR *psi;
	short sy;
	double masserrorbasin=0.0, masserror;
	double Pnbasin=0.0, Infbasin=0.0;
	FILE *f;                    /* file which contains the errors of simulation*/

	psi=new_doublevector(Nl);

	for(r=1;r<=Nr;r++){
		for(c=1;c<=Nc;c++){
			if(Z->co[r][c]!=NoV){

				sy=sl->type->co[r][c];

				/*control of net precipitation:*/
				if (wat->Pn->co[r][c]<-0.01){
					if(par->n_error<par->max_error){
						par->n_error++;
						f=fopen(error_file_name,"a");
						fprintf(f,"Pn[%ld,%ld] is negative: %f!\n",r,c,wat->Pn->co[r][c]);
						fclose(f);
						wat->Pn->co[r][c]=0.0;
					}
				}

				/*record psi_old*/
				for(l=1;l<=Nl;l++){
					psi->co[l]=sl->P->co[l][r][c];
				}

				/*solution of Richards' equation*/
				Richards_1D(Dt, r, c, sl, psi, &(wat->h_sup->co[r][c]), wat->Pn->co[r][c], time, par, &masserror, wat->q_sub);

 				if(sl->Jinf->co[r][c]!=sl->Jinf->co[r][c]){
					printf("ERROR ON INFILTRATION r:%ld c:%ld\n",r,c);
				}

				wat->error->co[r][c]+=masserror;
				masserrorbasin+=masserror/(double)par->total_pixel;
				Pnbasin+=wat->Pn->co[r][c]*Dt/(double)par->total_pixel;
				Infbasin+=sl->Jinf->co[r][c]*Dt/(double)par->total_pixel;

				/*write q_out for specified pixels*/
				for(i=1;i<=par->chkpt->nrh;i++){
					wat->out1->co[28][i]=masserror*3600.0/Dt;
				}

				//update P and theta
				for(l=1;l<=Nl;l++){
					sl->P->co[l][r][c]=psi->co[l];
					//sl->th->co[l][r][c]=theta_from_psi(l,r,c,sl,par);
					//if(sl->P->co[l][r][c]>1.E5) printf("-> %ld %ld %ld Psi:%f th:%f\n",l,r,c,sl->P->co[l][r][c],sl->th->co[l][r][c]);
				}

				//error check
				if (wat->h_sup->co[r][c]<0.0){
					if (wat->h_sup->co[r][c]<-1E-8){
						if(par->n_error<par->max_error){
							par->n_error++;
							f=fopen(error_file_name,"a");
							fprintf(f,"h_sup[%ld,%ld], at the end of vertical_water_balance subroutine, is negative: %f!\n",r,c,wat->h_sup->co[r][c]);
							fclose(f);
							printf("h_sup[%ld,%ld], at the end of vertical_water_balance subroutine, is negative: %f!\n",r,c,wat->h_sup->co[r][c]);
							printf("Inf:%f Pnet:%f\n",sl->Jinf->co[r][c],wat->Pn->co[r][c]);
						}
					}
					wat->h_sup->co[r][c]=0.0;
				}

				if (wat->h_sup->co[r][c]!=wat->h_sup->co[r][c]){
					wat->h_sup->co[r][c]=0.0;
					if(par->n_error<par->max_error){
						par->n_error++;
						f=fopen(error_file_name,"a");
						fprintf(f,"h_sup[%ld,%ld], at the end of vertical_water_balance subroutine, is not a number!\n",r,c);
						fclose(f);
					}
				}

			}
		}
	}

	free_doublevector(psi);

	wat->out2->co[8]+=masserrorbasin;

	f=fopen(error_file_name,"a");
	fprintf(f,"\nERROR MASS BALANCE: %20.18fmm/h - Pnet: %20.18fmm/h - Infiltration: %20.18fmm/h\n",masserrorbasin*3600.0/Dt,Pnbasin*3600.0/Dt,Infbasin*3600.0/Dt);
	fclose(f);

}


/*-----------------------------------------------------------------------------------------------------------------------------*/

void supflow(double Dt, double Dtmax, TOPO *top, LAND *land, WATER *wat, CHANNEL *cnet, PAR *par)
{
	long r,c,R,C,ch,s;                                    /* counters*/
	static short r_DD[11]={0,0,-1,-1,-1,0,1,1,1,-9999,0}; /* differential of number-pixel for rows and*/
	static short c_DD[11]={0,1,1,0,-1,-1,-1,0,1,-9999,0}; /* columns, depending on Drainage Directions*/
	double dx,dy;                                         /* the two dimensions of a pixel*/
	double Ks;											  /* the Strickler's coefficent calculated with a standard deviation*/
	double b[10];                                         /* area perpendicular to the superficial flow divided by h_sup*/
	double i;											  /*hydraulic gradient*/
	double q,tb,te,dt;
	short lu;

if(par->point_sim==0){	//distributed simulations

	initialize_doublevector(cnet->Qsup,0.0);
	initialize_doublevector(cnet->Qsup_spread,0.0);
	initialize_doublevector(cnet->Qsub,0.0);
	initialize_doublevector(cnet->Qsub_spread,0.0);

	dx=UV->U->co[1];                                     /*cell side [m]*/
	dy=UV->U->co[2];                                     /*cell side [m]*/

	b[1]=0.0;  b[2]=dy;             b[3]=dx/2.0+dy/2.0;  b[4]=dx;            b[5]=dx/2.0+dy/2.0;
	b[6]=dy;   b[7]=dx/2.0+dy/2.0;  b[8]=dx;             b[9]=dx/2.0+dy/2.0;

	te=0.0;

	do{

		tb=te;
		dt=Dt;

		//find dt min
		for(r=1;r<=Nr;r++){
			for(c=1;c<=Nc;c++){
				if(wat->h_sup->co[r][c]>0 && top->DD->co[r][c]>=1 && top->DD->co[r][c]<=8 && top->pixel_type->co[r][c]==0){
					lu=(short)land->LC->co[r][c];
					Ks=land->ty->co[lu][jcm];
					i=0.001*( wat->h_sup->co[r][c] - wat->h_sup->co[r+r_DD[top->DD->co[r][c]]][c+c_DD[top->DD->co[r][c]]] )/b[top->DD->co[r][c]+1] + top->i_DD->co[r][c];
					//i=top->i_DD->co[r][c];
					if(i<0) i=0;  //correct false slopes
					q=b[top->DD->co[r][c]+1]*Ks*pow(wat->h_sup->co[r][c]/1000.0,1.0+par->gamma_m)*sqrt(i)*1000.0/dx/dy;	//mm/s
					if(q>0){
						if(wat->h_sup->co[r][c]/q<dt) dt=wat->h_sup->co[r][c]/q;
					}
				}
			}
		}

		te=tb+dt;
		if(te>Dt){
			te=Dt;
			dt=te-tb;
		}

		/*wat->h_sup(=height of water over the land-surface) is updated adding the new runoff(=precipita-
		tion not infiltrated of the actual time); then it is calculated q_sub and it is checked
		that its value is not greater than the avaible water on the land-surface:*/
		/*Remember the units of measure: q_sup=[mm/s],b[m],Ks[m^(1/3)/s],wat->h_sup[mm],dx[m],dy[m]*/
		initialize_doublematrix(wat->q_sup,0.0);
		for(r=1;r<=Nr;r++){
			for(c=1;c<=Nc;c++){
				if(wat->h_sup->co[r][c]>0 && top->DD->co[r][c]>=1 && top->DD->co[r][c]<=8 && top->pixel_type->co[r][c]==0){
					lu=(short)land->LC->co[r][c];
					Ks=land->ty->co[lu][jcm];
					i=0.001*( wat->h_sup->co[r][c] - wat->h_sup->co[r+r_DD[top->DD->co[r][c]]][c+c_DD[top->DD->co[r][c]]] )/b[top->DD->co[r][c]+1] + top->i_DD->co[r][c];
					//i=top->i_DD->co[r][c];
					if(i<0) i=0.0;
					wat->q_sup->co[r][c]=b[top->DD->co[r][c]+1]*Ks*pow(wat->h_sup->co[r][c]/1000.0,1.0+par->gamma_m)*sqrt(i)*1000.0/dx/dy;

					if(wat->q_sup->co[r][c]!=wat->q_sup->co[r][c]){
						printf("NO VALUE SUP:%ld %ld %ld %ld %f %f\n",r,c,r+r_DD[top->DD->co[r][c]],c+c_DD[top->DD->co[r][c]],wat->h_sup->co[r][c],wat->h_sup->co[r+r_DD[top->DD->co[r][c]]][c+c_DD[top->DD->co[r][c]]]);
						printf("i:%f Dh:%f Ks:%f pow:%f iF:%f\n",i,wat->h_sup->co[r][c] - wat->h_sup->co[r+r_DD[top->DD->co[r][c]]][c+c_DD[top->DD->co[r][c]]],Ks,pow(wat->h_sup->co[r][c]/1000.0,1.0+par->gamma_m),top->i_DD->co[r][c]);
					}
				}
			}
		}

		/*After the computation of the surface flow,these flows are moved trough D8 scheme:*/
		for(r=1;r<=Nr;r++){
			for(c=1;c<=Nc;c++){
				if(top->pixel_type->co[r][c]==0){

					R=r+r_DD[top->DD->co[r][c]];
					C=c+c_DD[top->DD->co[r][c]];

					wat->h_sup->co[r][c]-=wat->q_sup->co[r][c]*dt;
					wat->h_sup->co[r][c]=Fmax(wat->h_sup->co[r][c], 0.0);

					//the superficial flow is added to the land pixels (code 0):
					if (top->pixel_type->co[R][C]==0){
						wat->h_sup->co[R][C]+=wat->q_sup->co[r][c]*dt;

					//the superficial flow is added to flow which flows into the channel pixels (code 10):
					}else if (top->pixel_type->co[R][C]==10){
						for(ch=1;ch<=cnet->r->nh;ch++){
							if(R==cnet->r->co[ch] && C==cnet->c->co[ch]){
								cnet->Qsup->co[ch]+=wat->q_sup->co[r][c]*dt/Dtmax;
								if(cnet->Qsup->co[ch]!=cnet->Qsup->co[ch]){
									printf("qsup no value: r:%ld c:%ld ch:%ld R:%ld C:%ld qsup:%f hsup:%f\n",r,c,ch,R,C,wat->q_sup->co[r][c],wat->h_sup->co[r][c]);
								}
							}
						}
					}

				}else if(top->pixel_type->co[r][c]==10){
					for(ch=1;ch<=cnet->r->nh;ch++){
						if(r==cnet->r->co[ch] && c==cnet->c->co[ch]){
							cnet->Qsub->co[ch]=wat->h_sup->co[r][c]/Dtmax; /*[mm/s]*/
							wat->h_sup->co[r][c]=0.0;
						}
					}
				}

			}
		}

		for(ch=1;ch<=cnet->r->nh;ch++){
			for(s=1;s<=cnet->Qsup_spread->nh;s++){
				cnet->Qsup_spread->co[s]+=(cnet->Qsup->co[ch])*(cnet->fraction_spread->co[ch][s])*0.001*dx*dy; /*in mc/s*/
				cnet->Qsub_spread->co[s]+=(cnet->Qsub->co[ch])*(cnet->fraction_spread->co[ch][s])*0.001*dx*dy; /*in mc/s*/
			}

		}


	}while(te<Dt);


}else{	//point simulation

	for(r=1;r<=Nr;r++){
		for(c=1;c<=Nc;c++){
			if(land->LC->co[r][c]!=NoV){
				q=0.0;
				lu=(short)land->LC->co[r][c];
				Ks=land->ty->co[lu][jcm];
				i=pow(pow(top->dz_dx->co[r][c],2.0)+pow(top->dz_dy->co[r][c],2.0),0.5);
				if(wat->h_sup->co[r][c]>0) q=Ks*pow(wat->h_sup->co[r][c]/1000.0,1.0+par->gamma_m)*sqrt(i)*1000.0;	//mm/s
				wat->h_sup->co[r][c]-=q*Dt;
				if(wat->h_sup->co[r][c]<0) wat->h_sup->co[r][c]=0.0;
				wat->h_sup->co[r][c]=0.0;
			}
		}
	}
}


}



/*--------------------------------------------*/


void subflow(double Dt, LAND *land, TOPO *top, SOIL *sl, PAR *par, WATER *wat){

	long l;  /*the index of depth is l not d because d is used for the vector of the sl-thickness*/
	long r,c,R,C;/*counters of rows and columns*/
	short sy, sy1; //sl type
	double dx,dy; /*the two dimensions of a pixel*/
	double ds,dn;
	double q;    /*auxiliar variable to calculate the subflow for unit of land-surface*/
	double i; //lateral gradient of pressure [-]
	double Kh, Kh1;

	dx=UV->U->co[1];	//m
	dy=UV->U->co[2];	//m

	for(r=1;r<=Nr;r++){
		for(c=1;c<=Nc;c++){
			if(land->LC->co[r][c]!=NoV){
				for(l=1;l<=Nl;l++){

					if(sl->P->co[l][r][c]!=sl->P->co[l][r][c]){
						printf("NOvalue Psi %ld %ld %ld\n",l,r,c);
						stop_execution();
					}

				}
			}
		}
	}

	initialize_doubletensor(wat->q_sub, 0.0);

	if(par->point_sim==0){	//2D simulations

		for(r=1;r<=Nr;r++){

			for(c=1;c<=Nc-1;c++){

				R=r;
				C=c+1;
				ds=dx;
				dn=dy;

				if((top->pixel_type->co[r][c]==0 || top->pixel_type->co[r][c]==10) && (top->pixel_type->co[R][C]==0 || top->pixel_type->co[R][C]==10)){

					sy=sl->type->co[r][c];
					sy1=sl->type->co[R][C];

					for(l=1;l<=Nl;l++){

						Kh=K(sl->P->co[l][r][c], sl->pa->co[sy][jKh][l], par->imp, sl->thice->co[l][r][c], sl->pa->co[sy][jsat][l],
									sl->pa->co[sy][jres][l], sl->pa->co[sy][ja][l], sl->pa->co[sy][jns][l], 1-1/sl->pa->co[sy][jns][l], sl->pa->co[sy][jv][l],
									sl->pa->co[sy][jpsimin][l], sl->T->co[l][r][c]);
						Kh1=K(sl->P->co[l][R][C], sl->pa->co[sy1][jKh][l], par->imp, sl->thice->co[l][R][C], sl->pa->co[sy1][jsat][l],
									sl->pa->co[sy1][jres][l], sl->pa->co[sy1][ja][l], sl->pa->co[sy1][jns][l], 1-1/sl->pa->co[sy1][jns][l], sl->pa->co[sy1][jv][l],
									sl->pa->co[sy1][jpsimin][l], sl->T->co[l][R][C]);

						i=top->dz_dx->co[r][c]+0.001*(sl->P->co[l][r][c]-sl->P->co[l][R][C])/ds;

						q=Mean(par->harm_or_arit_mean, ds, ds, Kh, Kh1)*i;	//[mm/s]

						wat->q_sub->co[l][r][c] += q;
						wat->q_sub->co[l][R][C] -= q;

					}
				}
			}
		}

		for(c=1;c<=Nc;c++){

			for(r=1;r<=Nr-1;r++){

				R=r+1;
				C=c;
				ds=dy;
				dn=dx;

				if((top->pixel_type->co[r][c]==0 || top->pixel_type->co[r][c]==10) && (top->pixel_type->co[R][C]==0 || top->pixel_type->co[R][C]==10)){

					sy=sl->type->co[r][c];
					sy1=sl->type->co[R][C];

					for(l=1;l<=Nl;l++){

						Kh=K(sl->P->co[l][r][c], sl->pa->co[sy][jKh][l], par->imp, sl->thice->co[l][r][c], sl->pa->co[sy][jsat][l],
									sl->pa->co[sy][jres][l], sl->pa->co[sy][ja][l], sl->pa->co[sy][jns][l], 1-1/sl->pa->co[sy][jns][l], sl->pa->co[sy][jv][l],
									sl->pa->co[sy][jpsimin][l], sl->T->co[l][r][c]);

						Kh1=K(sl->P->co[l][R][C], sl->pa->co[sy1][jKh][l], par->imp, sl->thice->co[l][R][C], sl->pa->co[sy1][jsat][l],
									sl->pa->co[sy1][jres][l], sl->pa->co[sy1][ja][l], sl->pa->co[sy1][jns][l], 1-1/sl->pa->co[sy1][jns][l], sl->pa->co[sy1][jv][l],
									sl->pa->co[sy1][jpsimin][l], sl->T->co[l][R][C]);

						i=top->dz_dy->co[r][c]+0.001*(sl->P->co[l][r][c]-sl->P->co[l][R][C])/ds;

						q=Mean(par->harm_or_arit_mean, ds, ds, Kh, Kh1)*i;	//[mm/s]

						wat->q_sub->co[l][r][c] += q;
						wat->q_sub->co[l][R][C] -= q;
					}
				}
			}
		}

	}else{	//1D simulations

		for(r=1;r<=Nr;r++){
			for(c=1;c<=Nc;c++){
				if(land->LC->co[r][c]!=NoV){
					sy=sl->type->co[r][c];
					for(l=1;l<=Nl;l++){
						Kh=K(sl->P->co[l][r][c], sl->pa->co[sy][jKh][l], par->imp, sl->thice->co[l][r][c], sl->pa->co[sy][jsat][l],
								sl->pa->co[sy][jres][l], sl->pa->co[sy][ja][l], sl->pa->co[sy][jns][l], 1-1/sl->pa->co[sy][jns][l], sl->pa->co[sy][jv][l],
								sl->pa->co[sy][jpsimin][l], sl->T->co[l][r][c]);
						i=top->dz_dx->co[r][c]+top->dz_dy->co[r][c];
						q=Kh*i;	//[mm/s]

						wat->q_sub->co[l][r][c] += q;

					}
				}
			}
		}
	}
}


/*--------------------------------------------*/
void routing(CHANNEL *cnet){

	long s;

	/*The just calculated Qsup_spread and Qsub_spread are added to the flow already
	present in the channel-network(Q_sup_s and Q_sub_s) and at once it is made a
	traslation of Q_sup_s e Q_sub_s towards the outlet:*/
	for(s=1;s<=cnet->Qsub_spread->nh-1;s++){
		cnet->Q_sub_s->co[s]=cnet->Q_sub_s->co[s+1]+cnet->Qsub_spread->co[s];
		cnet->Q_sup_s->co[s]=cnet->Q_sup_s->co[s+1]+cnet->Qsup_spread->co[s];
	}
	cnet->Q_sub_s->co[cnet->Qsub_spread->nh]=cnet->Qsub_spread->co[cnet->Qsub_spread->nh];
	cnet->Q_sup_s->co[cnet->Qsup_spread->nh]=cnet->Qsup_spread->co[cnet->Qsup_spread->nh];

}

/*--------------------------------------------*/
void output_waterbalance(double Dt, WATER *wat, SOIL *sl, PAR *par, DOUBLEMATRIX *Z){

	long i,l,r,c;

	for(i=1;i<=par->chkpt->nrh;i++){
		r=par->rc->co[i][1];
		c=par->rc->co[i][2];

		wat->out1->co[5][i]+=wat->Pn->co[r][c]*Dt;
		wat->out1->co[6][i]+=(wat->Pn->co[r][c]-sl->Jinf->co[r][c])*Dt;
		wat->out1->co[7][i]+=(wat->h_sup->co[r][c] - wat->out1->co[9][i] - (wat->Pn->co[r][c]-sl->Jinf->co[r][c])*Dt);    /*positive if entering into the pixel*/
		//wat->out1->co[8][i]+=sl->J->co[Nl][r][c]*Dt;
		wat->out1->co[9][i]=wat->h_sup->co[r][c];

		for(l=1;l<=Nl;l++){
			wat->out1->co[10][i]-=0.0*Dt;/*positive if entering into the pixel*/
		}
	}

	for(r=1;r<=Nr;r++){
		for(c=1;c<=Nc;c++){
			if(Z->co[r][c]!=NoV){ /*if the pixel is not a novalue*/
				wat->out2->co[3]+=wat->Pn->co[r][c]*Dt;								/*[mm]*/
				wat->out2->co[4]+=sl->Jinf->co[r][c]*Dt;							/*[mm]*/
			}
		}
	}

}


/*--------------------------------------------*/




void Richards_1D(double Dt, long r, long c, SOIL *sl, DOUBLEVECTOR *psi, double *h, double Pnet, double t, PAR *par, double *masslosscum, DOUBLETENSOR *q_sub)

{
	double Inf,massloss;
	DOUBLEVECTOR *e1;
	long l;

	*masslosscum=0.0;

	e1=new_doublevector(Nl);

	//initialization of the fluxes
	sl->Jinf->co[r][c]=0.0;

	Inf=Pnet+(*h)/Dt;

	solve_Richards_1D(r, c, Dt, &Inf, *h, psi, e1, &massloss, sl, par, q_sub);

	*masslosscum = *masslosscum + massloss;

	sl->Jinf->co[r][c]+=Inf;

	*h+=(Pnet-Inf)*Dt; /*in mm*/
	if(*h<0 && *h>-1.E-3) *h=0.0;
	if(*h<0){
		printf("hsup negative:%e %ld %ld Pnet:%f Inf:%f\n",*h,r,c,Pnet,Inf);
		stop_execution();
		*h=0.0;
	}

	//update psi
	for(l=1;l<=Nl;l++){
		psi->co[l]=e1->co[l];
	}

	/*Deallocate*/
	free_doublevector(e1);

}

/*--------------------------------------------*/


void find_coeff_Richards_1D(long r, long c, double dt, short *bc, double Inf, double h, DOUBLEVECTOR *psit, DOUBLEVECTOR *e0, DOUBLEVECTOR *adi,
	DOUBLEVECTOR *ad, DOUBLEVECTOR *ads, DOUBLEVECTOR *b, SOIL *sl, PAR *par, DOUBLETENSOR *q_sub){

	long l, m;
	short sy=sl->type->co[r][c];
	double dz, K1, Kdw, Kup, ddw, dup, dzdw, dzup, K1dw, K1up, C1, theta0, theta1;

	l=1;
	m=l;
	K1=K(e0->co[m], sl->pa->co[sy][jKv][m], par->imp, sl->thice->co[m][r][c], sl->pa->co[sy][jsat][m], sl->pa->co[sy][jres][m], sl->pa->co[sy][ja][m],
		sl->pa->co[sy][jns][m], 1-1/sl->pa->co[sy][jns][m], sl->pa->co[sy][jv][m], sl->pa->co[sy][jpsimin][l], sl->T->co[m][r][c]);
	dz=sl->pa->co[sy][jdz][m];

	m=l+1;
	Kdw=K(e0->co[m], sl->pa->co[sy][jKv][m], par->imp, sl->thice->co[m][r][c], sl->pa->co[sy][jsat][m], sl->pa->co[sy][jres][m], sl->pa->co[sy][ja][m],
		sl->pa->co[sy][jns][m], 1-1/sl->pa->co[sy][jns][m],sl->pa->co[sy][jv][m], sl->pa->co[sy][jpsimin][l], sl->T->co[m][r][c]);
	ddw=sl->pa->co[sy][jdz][m];

	dzdw=0.5*dz+0.5*ddw;

	K1dw=Mean(par->harm_or_arit_mean, dz, ddw, K1, Kdw);


	C1=dteta_dpsi(e0->co[l], sl->thice->co[l][r][c], sl->pa->co[sy][jsat][l], sl->pa->co[sy][jres][l], sl->pa->co[sy][ja][l],
		sl->pa->co[sy][jns][l], 1-1/sl->pa->co[sy][jns][l], par->psimin, par->Esoil);
	theta0=teta_psi(psit->co[l], sl->thice->co[l][r][c], sl->pa->co[sy][jsat][l], sl->pa->co[sy][jres][l], sl->pa->co[sy][ja][l],
		sl->pa->co[sy][jns][l], 1-1/sl->pa->co[sy][jns][l], par->psimin, par->Esoil);
	theta1=teta_psi(e0->co[l], sl->thice->co[l][r][c], sl->pa->co[sy][jsat][l], sl->pa->co[sy][jres][l], sl->pa->co[sy][ja][l],
		sl->pa->co[sy][jns][l], 1-1/sl->pa->co[sy][jns][l], par->psimin, par->Esoil);


	ad->co[l]= C1*dz/dt + K1dw/dzdw;
	ads->co[l]=-K1dw/dzdw;
	b->co[l]=(C1*e0->co[l]+theta0-theta1)*dz/dt - K1dw - q_sub->co[l][r][c];

	if(Inf > K1 + K1*(h-e0->co[l])/(dz/2.)){
		*bc=1;	//Dirichlet
		ad->co[l]+=K1/(dz/2.);
		b->co[l]+=(K1 + K1*h/(dz/2.));
	}else{
		*bc=0;	//Neumann
		b->co[l]+=Inf;
	}

	//middle layers
	for(l=2;l<=Nl-1;l++){

		m=l;
		K1=K(e0->co[m], sl->pa->co[sy][jKv][m], par->imp, sl->thice->co[m][r][c], sl->pa->co[sy][jsat][m], sl->pa->co[sy][jres][m],
			sl->pa->co[sy][ja][m], sl->pa->co[sy][jns][m], 1-1/sl->pa->co[sy][jns][m], sl->pa->co[sy][jv][m], sl->pa->co[sy][jpsimin][l], sl->T->co[m][r][c]);
		dz=sl->pa->co[sy][jdz][m];

		m=l+1;
		Kdw=K(e0->co[m], sl->pa->co[sy][jKv][m], par->imp, sl->thice->co[m][r][c], sl->pa->co[sy][jsat][m], sl->pa->co[sy][jres][m],
			sl->pa->co[sy][ja][m], sl->pa->co[sy][jns][m], 1-1/sl->pa->co[sy][jns][m], sl->pa->co[sy][jv][m], sl->pa->co[sy][jpsimin][l], sl->T->co[m][r][c]);
		ddw=sl->pa->co[sy][jdz][m];

		m=l-1;
		Kup=K(e0->co[m], sl->pa->co[sy][jKv][m], par->imp, sl->thice->co[m][r][c], sl->pa->co[sy][jsat][m], sl->pa->co[sy][jres][m],
			sl->pa->co[sy][ja][m], sl->pa->co[sy][jns][m], 1-1/sl->pa->co[sy][jns][m], sl->pa->co[sy][jv][m], sl->pa->co[sy][jpsimin][l], sl->T->co[m][r][c]);
		dup=sl->pa->co[sy][jdz][m];

		dzdw=0.5*dz+0.5*ddw;
		dzup=0.5*dz+0.5*dup;

		K1dw=Mean(par->harm_or_arit_mean, dz, ddw, K1, Kdw);
		K1up=Mean(par->harm_or_arit_mean, dz, dup, K1, Kup);


		C1=dteta_dpsi(e0->co[l], sl->thice->co[l][r][c], sl->pa->co[sy][jsat][l], sl->pa->co[sy][jres][l], sl->pa->co[sy][ja][l],
			sl->pa->co[sy][jns][l], 1-1/sl->pa->co[sy][jns][l], par->psimin, par->Esoil);
		theta0=teta_psi(psit->co[l], sl->thice->co[l][r][c], sl->pa->co[sy][jsat][l], sl->pa->co[sy][jres][l], sl->pa->co[sy][ja][l],
			sl->pa->co[sy][jns][l], 1-1/sl->pa->co[sy][jns][l], par->psimin, par->Esoil);
		theta1=teta_psi(e0->co[l], sl->thice->co[l][r][c], sl->pa->co[sy][jsat][l], sl->pa->co[sy][jres][l], sl->pa->co[sy][ja][l],
			sl->pa->co[sy][jns][l], 1-1/sl->pa->co[sy][jns][l], par->psimin, par->Esoil);


		ad->co[l]=C1*dz/dt + (K1dw/dzdw+K1up/dzup);
		ads->co[l]=-K1dw/dzdw;
		adi->co[l-1]=-K1up/dzup;
		b->co[l]=(C1*e0->co[l]+theta0-theta1)*dz/dt + (K1up-K1dw) - q_sub->co[l][r][c];

	}


	//last layer
	l=Nl;

	m=l;
	K1=K(e0->co[m], sl->pa->co[sy][jKv][m], par->imp, sl->thice->co[m][r][c], sl->pa->co[sy][jsat][m], sl->pa->co[sy][jres][m],
		sl->pa->co[sy][ja][m], sl->pa->co[sy][jns][m], 1-1/sl->pa->co[sy][jns][m], sl->pa->co[sy][jv][m], sl->pa->co[sy][jpsimin][l], sl->T->co[m][r][c]);
	dz=sl->pa->co[sy][jdz][m];

	m=l-1;
	Kup=K(e0->co[m], sl->pa->co[sy][jKv][m], par->imp, sl->thice->co[m][r][c], sl->pa->co[sy][jsat][m], sl->pa->co[sy][jres][m],
		sl->pa->co[sy][ja][m], sl->pa->co[sy][jns][m], 1-1/sl->pa->co[sy][jns][m], sl->pa->co[sy][jv][m], sl->pa->co[sy][jpsimin][l], sl->T->co[m][r][c]);
	dup=sl->pa->co[sy][jdz][m];

	dzup=0.5*dz+0.5*dup;

	K1up=Mean(par->harm_or_arit_mean, dz, dup, K1, Kup);


	C1=dteta_dpsi(e0->co[l], sl->thice->co[l][r][c], sl->pa->co[sy][jsat][l], sl->pa->co[sy][jres][l], sl->pa->co[sy][ja][l],
		sl->pa->co[sy][jns][l], 1-1/sl->pa->co[sy][jns][l], par->psimin, par->Esoil);
	theta0=teta_psi(psit->co[l], sl->thice->co[l][r][c], sl->pa->co[sy][jsat][l], sl->pa->co[sy][jres][l], sl->pa->co[sy][ja][l],
		sl->pa->co[sy][jns][l], 1-1/sl->pa->co[sy][jns][l], par->psimin, par->Esoil);
	theta1=teta_psi(e0->co[l], sl->thice->co[l][r][c], sl->pa->co[sy][jsat][l], sl->pa->co[sy][jres][l], sl->pa->co[sy][ja][l],
		sl->pa->co[sy][jns][l], 1-1/sl->pa->co[sy][jns][l], par->psimin, par->Esoil);

	ad->co[l]=C1*dz/dt + (K1up/dzup);
	adi->co[l-1]=-K1up/dzup;
	b->co[l]=(C1*e0->co[l]+theta0-theta1)*dz/dt + (K1up-K1*par->f_bound_Richards) - q_sub->co[l][r][c];

}


/*--------------------------------------------*/



void solve_Richards_1D(long r, long c, double dt, double *Inf, double h, DOUBLEVECTOR *psit, DOUBLEVECTOR *e1, double *massloss, SOIL *sl, PAR *par, DOUBLETENSOR *q_sub){

	double mass0, nw, massloss0, mass1, res, k, ActInf, Infmax, wlat;
	long cont, cont2, l;
	short sy=sl->type->co[r][c], bc, out, diff_bc;
	DOUBLEVECTOR *e0, *adi, *ad, *ads, *b, *de;

	ad=new_doublevector(Nl);
	adi=new_doublevector(Nl-1);
	ads=new_doublevector(Nl-1);
	b=new_doublevector(Nl);
	de=new_doublevector(Nl);
	e0=new_doublevector(Nl);

	*massloss=1.E99;

	mass0=0.0;
	for(l=1;l<=Nl;l++){
		mass0+=sl->pa->co[sy][jdz][l]*teta_psi(sl->P->co[l][r][c],sl->thice->co[l][r][c],sl->pa->co[sy][jsat][l],sl->pa->co[sy][jres][l],
				sl->pa->co[sy][ja][l],sl->pa->co[sy][jns][l],1-1/sl->pa->co[sy][jns][l],par->psimin, par->Esoil);
	}

	cont=0;

	for(l=1;l<=Nl;l++){
		e1->co[l]=psit->co[l];
	}

	do{ //loop of Picard iteration

		for(l=1;l<=Nl;l++){
			e0->co[l]=e1->co[l];
		}

		find_coeff_Richards_1D(r, c, dt, &bc, *Inf, h, psit, e0, adi, ad, ads, b, sl, par, q_sub);
		tridiag(2,r,c,Nl,adi,ad,ads,b,e1);

		cont++;

		for(l=1;l<=Nl;l++){
			de->co[l]=e1->co[l]-e0->co[l];
		}

		cont2=0;
		nw=1.0;
		massloss0=(*massloss);

		do{
			for(l=1;l<=Nl;l++){
				e1->co[l]=e0->co[l]+nw*de->co[l];
			}

			k=K(e0->co[1], sl->pa->co[sy][jKv][1], par->imp, sl->thice->co[1][r][c], sl->pa->co[sy][jsat][1], sl->pa->co[sy][jres][1],
				sl->pa->co[sy][ja][1], sl->pa->co[sy][jns][1], 1-1/sl->pa->co[sy][jns][1], sl->pa->co[sy][jv][1],
				sl->pa->co[sy][jpsimin][1], sl->T->co[1][r][c]);
			Infmax=k + k*(h-e1->co[1])/(0.5*sl->pa->co[sy][jdz][1]);
			ActInf=Fmin(*Inf, Infmax);

			//check consistency of boundary condition
			if( (bc==0 && *Inf>Infmax) || (bc==1 && *Inf<=Infmax) ){
				diff_bc=1;
			}else{
				diff_bc=0;
			}

			mass1=0.0;
			wlat=0.0;
			for(l=1;l<=Nl;l++){
				mass1+=sl->pa->co[sy][jdz][l]*teta_psi(e1->co[l],sl->thice->co[l][r][c],sl->pa->co[sy][jsat][l],sl->pa->co[sy][jres][l],
					sl->pa->co[sy][ja][l],sl->pa->co[sy][jns][l],1-1/sl->pa->co[sy][jns][l],par->psimin, par->Esoil);
				wlat-=q_sub->co[l][r][c]*dt;

			}
			*massloss=mass0-(mass1-ActInf*dt-wlat);

			cont2++;
			nw/=4.0;

			//printf("cont:%ld cont2:%ld Inf:%e Infmax:%e bc:%ld diff_bc:%ld massloss:%e\n",cont,cont2,*Inf,Infmax,bc,diff_bc,*massloss);

		}while(fabs(*massloss)>fabs(massloss0) && cont2<20);

		res=0.0;
		for(l=1;l<=Nl;l++){
			if(e1->co[l]<par->psimin) e1->co[l]=par->psimin;
			res+=pow((e1->co[l]-e0->co[l]),2.0);
		}
		res=pow(res,0.5);

		//printf("res:%f %f %f h:%f\n",res,e0->co[1],e1->co[1],h);

		out=0;
		if(res <= par->TolVWb) out=1;	//go out of the "do"
		if(cont >= par->MaxiterTol) out=1;
		if(fabs(*massloss) >= par->MaxErrWb) out=0;
		if(diff_bc > 0 || cont2 > 1) out=0;
		if(cont >= par->MaxiterErr) out=1;

	}while(out==0);

	/*if(fabs(*massloss)>par->MaxErrWb){
		//printf("Error too high %ld %ld massloss:%f Inf:%f Infpot:%f lat:%f bc:%ld psi1:%f e1:%f e0:%f\n",r,c,*massloss,ActInf,*Inf,wlat,bc,psit->co[1],e1->co[1],e0->co[1]);
		//printf("cont:%ld diff_bc:%ld res:%f mass0:%f mass1:%f inf:%f Inf:%e Infmax:%e k:%e\n",cont,diff_bc,res,mass0,mass1,ActInf*dt,*Inf,Infmax,k);
		//stop_execution();
	}*/

	*Inf=ActInf;

	free_doublevector(ad);
	free_doublevector(adi);
	free_doublevector(ads);
	free_doublevector(b);
	free_doublevector(de);
	free_doublevector(e0);

}
