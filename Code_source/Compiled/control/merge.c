

#include "m_pd.h"
#include "else_alloca.h"
#include <stdlib.h>

static t_class *merge_class;
static t_class* merge_inlet_class;

struct _merge_inlet;

typedef struct _merge{
    t_object              x_obj;
    int                   x_numinlets;
    int                   x_length;     // total length of all atoms from merge_inlet
    int                   x_trim;
    struct _merge_inlet*  x_ins;
}t_merge;

typedef struct _merge_inlet{
    t_class*    x_pd;
    t_atom*     x_atoms;
    int         x_numatoms;
    int         x_trig;
    int         x_id;
    t_merge*    x_owner;
}t_merge_inlet;

static void atoms_copy(int ac, t_atom *from, t_atom *to){
    for(int i = 0; i < ac; i++)
        to[i] = from[i];
}

static void merge_output(t_merge *x){
    t_atom * outatom;
    outatom = (t_atom *)getbytes(x->x_length * sizeof(t_atom));
    int offset = 0;
    for(int i = 0; i < x->x_numinlets; i++){
        int curnum = x->x_ins[i].x_numatoms; // number of atoms for current inlet
        atoms_copy(curnum, x->x_ins[i].x_atoms, outatom + offset); // copy them over to outatom
        offset += curnum;
    };
    if(x->x_trim && outatom->a_type == A_SYMBOL){
        t_symbol *selector = atom_getsymbolarg(0, x->x_length, outatom);
        outlet_anything(x->x_obj.ob_outlet, selector, x->x_length-1, outatom+1);
    }
    else
        outlet_list(x->x_obj.ob_outlet, &s_list, x->x_length, outatom);
    freebytes(outatom, x->x_length * sizeof(t_atom));
}

static void merge_inlet_atoms(t_merge_inlet *x, int ac, t_atom * av ){
    t_merge * owner = x->x_owner;
    freebytes(x->x_atoms, x->x_numatoms * sizeof(t_atom));
    owner->x_length -= x->x_numatoms;
    x->x_atoms = (t_atom *)getbytes(ac * sizeof(t_atom));
    owner->x_length += ac;
    x->x_numatoms = (t_int)ac;
    atoms_copy(ac, av, x->x_atoms);
}

static void merge_inlet_list(t_merge_inlet *x, t_symbol* s, int ac, t_atom* av){
    s = NULL;
    if(!ac){
        merge_output(x->x_owner);
        return;
    }
    merge_inlet_atoms(x, ac, av);
    if(x->x_trig == 1)
        merge_output(x->x_owner);
}

static void merge_inlet_anything(t_merge_inlet *x, t_symbol* s, int ac, t_atom* av){
    t_atom* at = ALLOCA(t_atom, ac + 1);
    SETSYMBOL(at, s);
    atoms_copy(ac, av, at+1);
    merge_inlet_list(x, 0, ac+1, at);
    FREEA(at, t_atom, ac + 1);
}

static void* merge_free(t_merge *x){
    for(int i = 0; i < x->x_numinlets; i++)
        freebytes(x->x_ins[i].x_atoms, x->x_ins[i].x_numatoms*sizeof(t_atom));
    freebytes(x->x_ins, x->x_numinlets * sizeof(t_merge_inlet));
    return(void *)free;
}

static void *merge_new(t_symbol *s, int ac, t_atom* av){
    t_merge *x = (t_merge *)pd_new(merge_class);
    t_symbol *dummy = s;
    dummy = NULL;
    t_float numinlets = 2;
    int hot = 0;
    x->x_trim = 0;
/////////////////////////////////////////////////////////////////////////////////////
    if(ac <= 3){
        int argnum = 0;
        while(ac > 0){
            if(av->a_type == A_FLOAT){
                t_float aval = atom_getfloatarg(0, ac, av);
                switch(argnum){
                    case 0:
                        numinlets = aval;
                        break;
                    case 1:
                        hot = (int)(aval != 0);
                        break;
                    default:
                        break;
                };
                ac--, av++;
                argnum++;
            }
            else if(av->a_type == A_SYMBOL && !argnum){
                if(atom_getsymbolarg(0, ac, av) == gensym("-trim")){
                    x->x_trim = 1;
                    ac--, av++;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        };
    }
/////////////////////////////////////////////////////////////////////////////////////
    int i;
    int n = (int)numinlets;
    x->x_numinlets = n < 2 ? 2 : n > 512 ? 512 : n;
    int * triggervals;
    triggervals = ALLOCA(int, x->x_numinlets);
    triggervals[0] = 1;
    if(hot){
        for(i = 0; i < x->x_numinlets; i++)
            triggervals[i] = 1;
    };
    x->x_ins = (t_merge_inlet *)getbytes(x->x_numinlets * sizeof(t_merge_inlet));
    x->x_length = x->x_numinlets;
    for(i = 0; i < x->x_numinlets; ++i){
        x->x_ins[i].x_pd    = merge_inlet_class;
        x->x_ins[i].x_atoms = (t_atom *)getbytes(1 * sizeof(t_atom));
        SETFLOAT(x->x_ins[i].x_atoms, 0);
        x->x_ins[i].x_numatoms = 1;
        x->x_ins[i].x_owner = x;
        x->x_ins[i].x_trig = triggervals[i];
        x->x_ins[i].x_id = i;
        inlet_new((t_object *)x, &(x->x_ins[i].x_pd), 0, 0);
    };
    outlet_new(&x->x_obj, &s_list);
    FREEA(triggervals, int, x->x_numinlets);
    return(x);
errstate:
    pd_error(x, "[merge]: improper args");
    return(NULL);
}

extern void merge_setup(void){
    t_class* c = NULL;
    c = class_new(gensym("merge-inlet"), 0, 0, sizeof(t_merge_inlet), CLASS_PD, 0);
    if(c){
        class_addlist(c, (t_method)merge_inlet_list);
        class_addanything(c, (t_method)merge_inlet_anything);
    }
    merge_inlet_class = c;
    c = class_new(gensym("merge"), (t_newmethod)merge_new, (t_method)merge_free,
        sizeof(t_merge), CLASS_NOINLET, A_GIMME, 0);
    merge_class = c;
}
