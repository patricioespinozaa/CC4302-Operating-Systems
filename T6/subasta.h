// Ud. debe implementar struct subasta y las funciones nuevaSubasta,
// ofrecer, adjudicar y destruirSubasta
typedef struct subasta *Subasta;

Subasta nuevaSubasta(int unidades);
int ofrecer(Subasta s, double precio);
double adjudicar(Subasta s, int *punidades);
void destruirSubasta(Subasta s);
