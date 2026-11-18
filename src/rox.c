#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define META ".rox"
#define P 4096

static void die(const char *s){perror(s);exit(1);}
static int exists(const char *p){struct stat st;return stat(p,&st)==0;}
static int safe(const char *p){
    return p&&*p&&p[0]!='/'&&!strstr(p,"../")&&strcmp(p,"..")&&
        !strstr(p,"/..")&&strcmp(p,META)&&strncmp(p,META "/",5)!=0;
}
static int mkdirs(const char *p){
    char b[P];if(strlen(p)>=sizeof b)return -1;strcpy(b,p);
    for(char *q=b+1;*q;q++)if(*q=='/'){*q=0;if(mkdir(b,0755)&&errno!=EEXIST)return -1;*q='/';}
    if(mkdir(b,0755)&&errno!=EEXIST)return -1;return 0;
}
static int copy(const char *a,const char *b){
    FILE *i=fopen(a,"rb");if(!i)return -1;char d[P];if(strlen(b)>=sizeof d){fclose(i);return -1;}
    strcpy(d,b);char *s=strrchr(d,'/');if(s){*s=0;if(mkdirs(d)){fclose(i);return -1;}}
    FILE *o=fopen(b,"wb");if(!o){fclose(i);return -1;}char buf[8192];size_t n;int r=0;
    while((n=fread(buf,1,sizeof buf,i)))if(fwrite(buf,1,n,o)!=n){r=-1;break;}
    if(ferror(i)||fclose(o))r=-1;fclose(i);return r;
}
static int repo(void){return exists(META "/HEAD")&&exists(META "/commits");}
static void head(char *b,size_t n){FILE *f=fopen(META "/HEAD","r");if(!f){*b=0;return;}if(!fgets(b,(int)n,f))*b=0;fclose(f);b[strcspn(b,"\r\n")]=0;}
static int listed(const char *p){FILE *f=fopen(META "/index","r");if(!f)return 0;char b[P];int yes=0;while(fgets(b,sizeof b,f)){b[strcspn(b,"\r\n")]=0;if(!strcmp(b,p)){yes=1;break;}}fclose(f);return yes;}
static int add(const char *p){
    if(!safe(p)){fprintf(stderr,"rox: unsafe path: %s\n",p);return 1;}
    struct stat st;if(stat(p,&st)||!S_ISREG(st.st_mode)){fprintf(stderr,"rox: not a regular file: %s\n",p);return 1;}
    char dst[P];snprintf(dst,sizeof dst,META "/stage/%s",p);if(copy(p,dst))die("rox: staging");
    if(!listed(p)){FILE *f=fopen(META "/index","a");if(!f)die("rox: index");fprintf(f,"%s\n",p);fclose(f);}
    printf("added %s\n",p);return 0;
}
static int init(void){
    if(exists(META)){fprintf(stderr,"rox: repository already exists\n");return 1;}
    if(mkdirs(META "/commits")||mkdirs(META "/stage"))die("rox: init");
    FILE *f=fopen(META "/HEAD","w");if(!f)die("rox: HEAD");fclose(f);
    f=fopen(META "/index","w");if(!f)die("rox: index");fclose(f);
    f=fopen(META "/config","w");if(!f)die("rox: config");fputs("format=1\n",f);fclose(f);
    puts("Initialized empty ROX repository.");return 0;
}
static int commit(const char *msg){
    if(!repo()){fprintf(stderr,"rox: not a repository\n");return 1;}
    char h[128];head(h,sizeof h);int id=1;DIR *d=opendir(META "/commits");struct dirent *e;
    if(d){while((e=readdir(d))){int x=atoi(e->d_name);if(x>=id)id=x+1;}closedir(d);}
    char dir[P],files[P],src[P],dst[P],path[P],meta[P];
    snprintf(dir,sizeof dir,META "/commits/%06d",id);snprintf(files,sizeof files,"%s/files",dir);if(mkdirs(files))die("rox: commit");
    if(h[0]){
        char prev[P];snprintf(prev,sizeof prev,META "/commits/%s/files",h);DIR *pd=opendir(prev);
        if(pd){while((e=readdir(pd))){if(e->d_name[0]=='.')continue;snprintf(src,sizeof src,"%s/%s",prev,e->d_name);snprintf(dst,sizeof dst,"%s/%s",files,e->d_name);if(copy(src,dst)){} }closedir(pd);}
    }
    FILE *idx=fopen(META "/index","r");if(!idx)die("rox: index");int count=0;
    while(fgets(path,sizeof path,idx)){path[strcspn(path,"\r\n")]=0;if(!safe(path)){fclose(idx);return 1;}snprintf(src,sizeof src,META "/stage/%s",path);snprintf(dst,sizeof dst,"%s/%s",files,path);if(copy(src,dst))die("rox: snapshot");count++;}
    fclose(idx);if(!count){fprintf(stderr,"rox: nothing staged\n");return 1;}
    snprintf(meta,sizeof meta,"%s/message",dir);FILE *f=fopen(meta,"w");if(!f)die("rox: message");fprintf(f,"%s\n",msg);fclose(f);
    snprintf(meta,sizeof meta,"%s/parent",dir);f=fopen(meta,"w");if(!f)die("rox: parent");fprintf(f,"%s\n",h);fclose(f);
    f=fopen(META "/HEAD","w");if(!f)die("rox: HEAD");fprintf(f,"%06d\n",id);fclose(f);
    f=fopen(META "/index","w");if(!f)die("rox: index");fclose(f);
    printf("[%06d] %s\n",id,msg);return 0;
}
static int status(void){
    if(!repo()){fprintf(stderr,"rox: not a repository\n");return 1;}char h[128];head(h,sizeof h);printf("HEAD: %s\n",h[0]?h:"(no commits)");
    FILE *f=fopen(META "/index","r");char b[P];int n=0;if(f){while(fgets(b,sizeof b,f)){b[strcspn(b,"\r\n")]=0;printf("A  %s\n",b);n++;}fclose(f);}if(!n)puts("No staged files.");return 0;
}
static int logbook(void){
    if(!repo()){fprintf(stderr,"rox: not a repository\n");return 1;}char h[128];head(h,sizeof h);if(!h[0]){puts("No commits.");return 0;}
    while(h[0]){char p[P],parent[128]="",msg[1024]="";snprintf(p,sizeof p,META "/commits/%s/message",h);FILE *f=fopen(p,"r");if(!f)break;fgets(msg,sizeof msg,f);fclose(f);msg[strcspn(msg,"\r\n")]=0;printf("commit %s\n    %s\n\n",h,msg);snprintf(p,sizeof p,META "/commits/%s/parent",h);f=fopen(p,"r");if(!f)break;fgets(parent,sizeof parent,f);fclose(f);parent[strcspn(parent,"\r\n")]=0;strcpy(h,parent);}return 0;
}
static char **lines(const char *p,size_t *n){
    *n=0;FILE *f=p?fopen(p,"r"):NULL;if(!f)return NULL;char **v=NULL,*s=NULL;size_t cap=0;ssize_t z;
    while((z=getline(&s,&cap,f))!=-1){char *q=malloc((size_t)z+1);if(!q)exit(1);memcpy(q,s,(size_t)z+1);v=realloc(v,(*n+1)*sizeof *v);v[(*n)++]=q;}free(s);fclose(f);return v;
}
static void release(char **v,size_t n){for(size_t i=0;i<n;i++)free(v[i]);free(v);}
static void patch_file(const char *p,const char *oldp,const char *newp){
    size_t a=0,b=0;char **o=lines(oldp,&a),**v=lines(newp,&b);size_t common=a<b?a:b,add=b>a?b-a:0,del=a>b?a-b:0,mod=0;
    for(size_t i=0;i<common;i++)if(strcmp(o[i],v[i]))mod++;
    printf("%s       +%zu -%zu M%zu\n^^^\n",p,add,del,mod);
    for(size_t i=0;i<common;i++){if(!strcmp(o[i],v[i]))printf(" %s",v[i]);else{printf("M-%s",o[i]);if(o[i][strlen(o[i])-1]!='\n')putchar('\n');printf("M+%s",v[i]);if(v[i][strlen(v[i])-1]!='\n')putchar('\n');}}
    for(size_t i=common;i<a;i++){printf("-%s",o[i]);if(o[i][strlen(o[i])-1]!='\n')putchar('\n');}
    for(size_t i=common;i<b;i++){printf("+%s",v[i]);if(v[i][strlen(v[i])-1]!='\n')putchar('\n');}
    puts("^^^");release(o,a);release(v,b);
}
static int diff(void){
    char h[128];head(h,sizeof h);FILE *f=fopen(META "/index","r");if(!f)return 0;char p[P];int n=0;
    while(fgets(p,sizeof p,f)){p[strcspn(p,"\r\n")]=0;char a[P],b[P];snprintf(a,sizeof a,META "/stage/%s",p);if(h[0])snprintf(b,sizeof b,META "/commits/%s/files/%s",h,p);else b[0]=0;patch_file(p,b[0]&&exists(b)?b:NULL,a);n++;}fclose(f);if(!n)fprintf(stderr,"rox: no staged changes\n");return 0;
}
static int apply(const char *name){
    FILE *f=fopen(name,"r");if(!f)die("rox: patch");char *s=NULL;size_t cap=0;ssize_t n;if(getline(&s,&cap,f)<0){fclose(f);free(s);return 1;}s[strcspn(s,"\r\n")]=0;char message[1024];snprintf(message,sizeof message,"%s",s);
    while((n=getline(&s,&cap,f))!=-1){
        s[strcspn(s,"\r\n")]=0;if(!*s)continue;char path[P];size_t ac,dc,mc;
        if(sscanf(s,"%4095s +%zu -%zu M%zu",path,&ac,&dc,&mc)!=4||!safe(path)){fprintf(stderr,"rox: invalid file header\n");goto bad;}
        if(getline(&s,&cap,f)<0||strcmp(s,"^^^\n")&&strcmp(s,"^^^")){fprintf(stderr,"rox: missing ^^^ marker\n");goto bad;}
        size_t oldn=0,pos=0,outn=0;char **old=lines(path,&oldn),**out=NULL;int closed=0;
        while((n=getline(&s,&cap,f))!=-1){
            if(!strcmp(s,"^^^\n")||!strcmp(s,"^^^")){closed=1;break;}
            s[strcspn(s,"\r\n")]=0;
            if(!strncmp(s,"M-",2)){
                char *expected=strdup(s+2);if(getline(&s,&cap,f)<0||strncmp(s,"M+",2)){free(expected);goto filebad;}
                if(pos>=oldn||strcmp(old[pos],expected)){free(expected);fprintf(stderr,"rox: modified old line mismatch: %s\n",path);goto filebad;}free(expected);
                out=realloc(out,(outn+1)*sizeof *out);out[outn++]=strdup(s+2);pos++;continue;
            }
            if(s[0]=='-'){if(pos>=oldn||strcmp(old[pos],s+1)){fprintf(stderr,"rox: deleted line mismatch: %s\n",path);goto filebad;}pos++;continue;}
            if(s[0]=='+'){out=realloc(out,(outn+1)*sizeof *out);out[outn++]=strdup(s+1);continue;}
            if(s[0]==' '){if(pos>=oldn||strcmp(old[pos],s+1)){fprintf(stderr,"rox: context mismatch: %s\n",path);goto filebad;}out=realloc(out,(outn+1)*sizeof *out);out[outn++]=strdup(old[pos++]);continue;}
            fprintf(stderr,"rox: unknown line marker: %s\n",path);goto filebad;
        }
        if(!closed||pos!=oldn){fprintf(stderr,"rox: incomplete patch: %s\n",path);goto filebad;}
        {char tmp[P];snprintf(tmp,sizeof tmp,"%s.rox-tmp",path);char *slash=strrchr(tmp,'/');if(slash){*slash=0;mkdirs(tmp);*slash='/';}FILE *o=fopen(tmp,"w");if(!o)die("rox: output");for(size_t i=0;i<outn;i++)fputs(out[i],o);if(fclose(o)||rename(tmp,path))die("rox: replace");}
        release(old,oldn);release(out,outn);continue;
filebad: release(old,oldn);release(out,outn);goto bad;
    }
    fclose(f);free(s);printf("Applied patch: %s\n",message);return 0;
bad:fclose(f);free(s);return 1;
}
static void usage(void){puts("Rox - local version control\nrox init\nrox add <file>...\nrox commit -m <message>\nrox status\nrox log\nrox diff\nrox apply <patch.rpt>");}
int main(int argc,char **argv){
    if(argc<2){usage();return 1;}
    if(!strcmp(argv[1],"init"))return init();
    if(!strcmp(argv[1],"add")){if(!repo())return fprintf(stderr,"rox: not a repository\n"),1;if(argc<3)return fprintf(stderr,"rox: add needs files\n"),1;int r=0;for(int i=2;i<argc;i++)if(!strcmp(argv[i],".")||add(argv[i]))r=1;return r;}
    if(!strcmp(argv[1],"commit"))return argc==4&&!strcmp(argv[2],"-m")?commit(argv[3]):(fprintf(stderr,"rox: use commit -m <message>\n"),1);
    if(!strcmp(argv[1],"status"))return status();
    if(!strcmp(argv[1],"log"))return logbook();
    if(!strcmp(argv[1],"diff"))return diff();
    if(!strcmp(argv[1],"apply"))return argc==3?apply(argv[2]):(fprintf(stderr,"rox: use apply <patch.rpt>\n"),1);
    usage();return 1;
}