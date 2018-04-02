#include<stdio.h>
#include<string.h>

/*Red Black Tree*/
typedef struct rbtnode{
    int color; //Red: 0 Black: 1
    char begin[15];
    char end[15];
    struct rbtnode *left;
    struct rbtnode *right;
    struct rbtnode *parent;
}rbtnode;

/*Left Rotate*/
/*
 *        px                       px
 *        /                        /
 *       x                        y
 *      / \       --left-->      / \
 *     lx  y                    x  ry
 *        / \                  / \
 *       lx ly                lx ly
 */
void leftRotate(rbtnode *root, rbtnode *x)
{
    rbtnode *y=x->right;
    x->right=y->left;
    if(y->left!=NULL) y->left->parent=x;
    y->parent=x->parent;
    if(x->parent==NULL){
        root=y;
    }
    else{
        if(x->parent->left==x) x->parent->left=y;
        else x->parent->right=y;
    }
    y->left=x;
    x->parent=y;
}

/*Right Rotate*/
/*
 *       py                     py
 *       /                      /
 *      y                      x
 *     / \     --right-->     / \
 *    x  ry                  lx  y
 *   / \                        / \
 *  lx rx                      rx ry
 */
void rightRotate(rbtnode *root, rbtnode *y){
    rbtnode *x=y->left;
    y->left=x->right;
    if(x->right!=NULL) x->right->parent=y;
    x->parent=y->parent;
    if(y->parent==NULL){
        root=x;
    }
    else{
        if(y==y->parent->right) y->parent->right=x;
        else y->parent->left=x;
    }
    x->right=y;
    y->parent=x;
}

/*Check whether the time can be added*/
int addable(rbtnode *root, char *begin, char *end)
{
    rbtnode *cur=root;
    while(cur!=NULL){
        if(strcmp(end,cur->begin)<=0){
            cur=cur->left;
        }
        else if(strcmp(cur->end,begin)<=0){
            cur=cur->right;
        }
        else return 0;
    }
    return 1;
}

char* minstr(char *a, char *b)
{
	static char *ans;
	if(strcmp(a,b)<0) strcpy(ans,a);
	else strcpy(ans,b);
	return ans;
}

char* maxstr(char *a, char *b)
{
	static char *ans;
	if(strcmp(a,b)>0) strcpy(ans,a);
	else strcpy(ans,b);
	return ans;
}

void conflict(rbtnode *node, char *begin, char *end, char *period)
{
	if(strcmp(begin,end)<=0) return ;
	rbtnode *cur=node;
	while(cur!=NULL){
		if(strcmp(end,cur->begin)<=0){
            cur=cur->left;
        }
        else if(strcmp(cur->end,begin)<=0){
            cur=cur->right;
        }
        else{
			strcat(period, maxstr(begin,cur->begin));
			strcat(period, " ");
			strcat(period, minstr(end,cur->end));
			strcat(period, " ");
			conflict(cur, begin, maxstr(begin, cur->begin), period);
			conflict(cur, minstr(end, cur->end), end, period);
			return ;
		}
	}
}

void insert(rbtnode *root, char *begin, char *end)
{
    rbtnode *node=(rbtnode*)malloc(sizeof(rbtnode));
    strcpy(node->begin,begin);
    strcpy(node->end,end);
    node->left=NULL;
    node->right=NULL;
    rbtnode *y=NULL;
    rbtnode *x=root;

    while(x!=NULL){
        y=x;
        if(strcmp(node->end,x->begin)<=0) x=x->left;
        else x=x->right;
    }
    node->parent=y;
    if(y!=NULL){
        if(strcmp(node->end,y->begin)<=0) y->left=node;
        else y->right=node;
    }
    else root=node;
    node->color=0;
    insert_fixup(root,node);
}

void insert_fixup(rbtnode *root, rbtnode *node)
{
    rbtnode *parent,*gparent;
    while((parent=node->parent) && (parent->color==0)){
        gparent=parent->parent;
        if(parent==gparent->left){
            rbtnode *uncle=gparent->right;
            if(uncle && uncle->color==0){
                uncle->color=1;
                parent->color=1;
                gparent->color=0;
                node=gparent;
                continue;
            }
            if(parent->right==node){
                rbtnode *temp;
                leftRotate(root,parent);
                temp=parent;
                parent=node;
                node=temp;
            }
            parent->color=1;
            gparent->color=0;
            rightRotate(root,gparent);
        }
        else{
            rbtnode *uncle=gparent->left;
            if(uncle && uncle->color==0){
                uncle->color=1;
                parent->color=1;
                gparent->color=0;
                node=gparent;
                continue;
            }
            if(parent->left==node){
                rbtnode *temp;
                rightRotate(root,parent);
                temp=parent;
                parent=node;
                node=temp;
            }
            parent->color=1;
            gparent->color=0;
            leftRotate(root,gparent);
        }
    }
    root->color=1;
}

void freetree(rbtnode *node)
{
    if(node==NULL) return;
    freetree(node->left);
    freetree(node->right);
    free(node);
}

/*---Grandchild---*/
void grandchildProcess(int cToG[2], int gToC[2])
{
    rbtnode *root=NULL;
    close(cToG[1]);
    close(gToC[0]);
    while(1){
        char se[80];
		memset(se,0,sizeof(se));
        n = read(cToG[0], se, 80);
        se[n] = 0;
        if(se[0] == 'c'){
            // close the process
            freetree(root);
            close(cToG[0]);
            close(gToC[0]);
            exit(0);
        }
        char *seq = strtok(se, " "), begin[80], end[80];
        strcpy(begin, seq[0]);
        strcpy(end, seq[1]);
        if(addable(root,begin,end)){
            write(gToC[1],"Y",3); //I have spare time to join
        }
        else{
			char period[80];
			memset(period,0,sizeof(period));
			conflict(root, begin, end, period);
			char con[90];
			sprintf(con,"N %s",period);
			write(gToC[1],con,strlen(con)); //I do not have time
		} 
        char buf[80];
        read(cToG[0],buf,80);
        if(buf[0]=='Y'){
            //add it to my time schedule
            insert(root,begin,end);
        }
    }
}