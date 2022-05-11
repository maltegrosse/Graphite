#ifndef __IMPUTEVCF
#define __IMPUTEVCF

#include <map>
#include <set>
#include "Pedigree.h"
#include "Map.h"

class Option;
class VCFFamily;
class VCFHeteroHomo;


//////////////////// Materials ////////////////////

class Materials {
	const PedigreeTable	*pedigree;
	const Map	*geno_map;
	std::vector<std::pair<std::string,std::string>>	families;
	
public:
	Materials(const PedigreeTable *p, const Map *m,
				const std::vector<std::pair<std::string,std::string>>& f) :
									pedigree(p), geno_map(m), families(f) { }
	~Materials();
	
	const PedigreeTable& get_ped() const { return *pedigree; }
	const Map& get_map() const { return *geno_map; }
	const std::vector<std::pair<std::string,std::string>>&
	get_families() const { return families; }
	
	void select_families(
				std::set<std::pair<std::string,std::string>>& set_families);
	
public:
	static Materials *create(const Option *option);
};


//////////////////// process ////////////////////

typedef std::pair<std::string,std::string>	Parents;
typedef std::map<std::pair<Parents,bool>, VCFHeteroHomo *>	HeteroParentVCFs;

void select_families(Materials *materials, const HeteroParentVCFs& vcfs);
HeteroParentVCFs extract_VCFs(const Materials *mat, const Option *option);
VCFFamily *impute_each(const Parents& parents, const Map& gmap,
										const HeteroParentVCFs& vcfs, int T);
std::vector<VCFFamily *> impute(const HeteroParentVCFs& vcfs,
									Materials *mat, const Option *option);

#endif