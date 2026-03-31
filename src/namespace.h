#ifndef MINNAS_NS_H
#define MINNAS_NS_H
typedef struct NamespaceMgr NamespaceMgr;
NamespaceMgr *nsmgr_create(const char *repo_root, CAS *cas);
void nsmgr_free(NamespaceMgr *nm);
int nsmgr_create_namespace(NamespaceMgr *nm, const char *name);
int nsmgr_delete_namespace(NamespaceMgr *nm, const char *name);
char **nsmgr_list_namespaces(NamespaceMgr *nm, int *count);
int nsmgr_switch_namespace(NamespaceMgr *nm, const char *name);
char *nsmgr_get_current(NamespaceMgr *nm);  // caller frees
char *nsmgr_get_current_tree_sha(NamespaceMgr *nm);
int nsmgr_set_current_tree_sha(NamespaceMgr *nm, const char *sha);
#endif
