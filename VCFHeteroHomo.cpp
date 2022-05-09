#include <algorithm>
#include <cmath>
#include <cassert>
#include "common.h"
#include "VCFHeteroHomo.h"
#include "VCFOriginal.h"
#include "Map.h"
#include "Pedigree.h"
#include "BiasProbability.h"

using namespace std;


//////////////////// VCFHeteroHomoRecord ////////////////////

VCFHeteroHomoRecord *VCFHeteroHomoRecord::copy() const {
	return new VCFHeteroHomoRecord(this->v, this->samples);
}

vector<int> VCFHeteroHomoRecord::get_int_gts() const {
	vector<int>	gts(samples.size());
	for(size_t i = 0U; i < samples.size(); ++i)
		gts[i] = get_int_gt(i);
	return gts;
}

vector<vector<double>> VCFHeteroHomoRecord::make_probability_table() const {
	vector<vector<double>>	pss_(3, vector<double>(3));
	pss_[0][0] = 0.5;  pss_[0][1] = 0.5;  pss_[0][2] = 0.0;
	pss_[1][0] = 0.25; pss_[1][1] = 0.5;  pss_[1][2] = 0.25;
	pss_[2][0] = 0.0;  pss_[2][1] = 0.5;  pss_[2][2] = 0.5;
	
	vector<vector<double>>	pss(3, vector<double>(3));
	const double	p_miss = 0.01;
	for(int i = 0; i < 3; ++i) {
		for(int j = 0; j < 3; ++j)
			pss[i][j] = (pss_[i][j] + p_miss) / (1.0 + 3 * p_miss);
	}
	return pss;
}

vector<int> VCFHeteroHomoRecord::count_numbers() const {
	vector<int>	ns(3);
	for(size_t i = 2U; i < samples.size(); ++i) {
		const int	int_gt = get_int_gt(i);
		if(int_gt >= 0)
			ns[int_gt] += 1;
	}
	return ns;
}

vector<double> VCFHeteroHomoRecord::log_likelihood(
								const vector<vector<double>>& pss,
								const vector<int>& ns) const {
	vector<double>	lls(3, 0.0);
	for(int i = 0; i < 3; ++i) {
		for(int j = 0; j < 3; ++j)
			lls[i] += ns[j] * log(pss[i][j]);
	}
	return lls;
}

VCFHeteroHomoRecord::SEGTYPE VCFHeteroHomoRecord::segregation_type() const {
	const vector<vector<double>>	pss = make_probability_table();
	const vector<int>				ns = count_numbers();
	const vector<double>			lls = log_likelihood(pss, ns);
	
	if(ns[0] + ns[1] == 0 || ns[0] + ns[2] == 0 || ns[1] + ns[2] == 0)
		return SEGTYPE::None;
	
	const auto	max_iter = max_element(lls.begin(), lls.end());
	if(lls[0] == *max_iter)
		return SEGTYPE::HomoHetero;
	else if(lls[1] == *max_iter)
		return SEGTYPE::HeteroHetero;
	else
		return SEGTYPE::HeteroHomo;
}

bool VCFHeteroHomoRecord::is_Mendelian_segregation() const {
	const auto	seg_type = segregation_type();
	if(seg_type == SEGTYPE::None)
		return false;
	
	const int	gt_m = this->mat_int_gt();
	const int	gt_p = this->pat_int_gt();
	if(gt_m == -1 || gt_p == -1)
		return false;
	
	switch(seg_type) {
	case SEGTYPE::HomoHetero:	return gt_m + gt_p == 1;
	case SEGTYPE::HeteroHetero:	return gt_m == 1 && gt_p == 1;
	default:					return gt_m + gt_p == 3;
	}
}

bool VCFHeteroHomoRecord::is_hetero_and_homo(bool is_mat) const {
	if(!is_Mendelian_segregation())
		return false;
	
	const int	gt_m = this->mat_int_gt();
	const int	gt_p = this->pat_int_gt();
	if(is_mat)	// is mat hetero?
		return gt_m == 1 && (gt_p == 0 || gt_p == 2);
	else
		return (gt_m == 0 || gt_m == 2) && gt_p == 1;
}

int VCFHeteroHomoRecord::genotype_from_hetero_parent(int i, int homo_gt) const {
	const int	gt_ = get_int_gt(i);
	const int	gt = gt_ - homo_gt / 2;
	if(gt == 0 || gt == 1)
		return gt;
	else
		return -1;
}

vector<int> VCFHeteroHomoRecord::genotypes_from_hetero_parent(
											bool is_mat_hetero) const {
	vector<int>	gts;
	const int	homo_gt = is_mat_hetero ? pat_int_gt() : mat_int_gt();
	for(size_t i = 2U; i < samples.size(); ++i) {
		gts.push_back(genotype_from_hetero_parent(i, homo_gt));
	}
	return gts;
}

bool VCFHeteroHomoRecord::is_valid_segregation(bool is_mat, double cM) const {
	const vector<int>	gts = genotypes_from_hetero_parent(is_mat);
	int	N = 0;
	int	n0 = 0;
	for(auto p = gts.begin(); p != gts.end(); ++p) {
		if(*p != -1)
			N += 1;
		if(*p == 0)
			n0 += 1;
	}
	const int	bias = std::min(n0, N - n0);
	BiasProbability	*bias_probability = BiasProbability::getInstance();
	return bias >= bias_probability->compute_max_bias(N, cM);
}


//////////////////// VCFHeteroHomo ////////////////////

VCFHeteroHomo::VCFHeteroHomo(const vector<STRVEC>& h, const STRVEC& s,
							vector<VCFHeteroHomoRecord *> rs, const Map& m) :
										VCFFamily(h, s, to_VCFFamilyRecord(rs)),
										records(rs), genetic_map(m) { }

VCFHeteroHomo::~VCFHeteroHomo() {
#if 1
	for(auto p = records.begin(); p != records.end(); ++p)
		delete *p;
#else
	for(auto p = records.begin(); p != records.end(); ++p) {
		cout << p - records.begin() << endl;
		delete *p;
	}
#endif
}

vector<VCFFamilyRecord *> VCFHeteroHomo::to_VCFFamilyRecord(
										vector<VCFHeteroHomoRecord *>& rs) {
	vector<VCFFamilyRecord *>	records;
	for(auto p = rs.begin(); p != rs.end(); ++p) {
		VCFHeteroHomoRecord	*r = *p;
		auto	*record = new VCFFamilyRecord(r->get_v(), r->get_samples());
		records.push_back(record);
	}
	return records;
}

double VCFHeteroHomo::cM(size_t i) const {
	return genetic_map.bp_to_cM(records[i]->pos());
}

// -> (distance, inversion)
pair<int,bool> VCFHeteroHomo::distance(const vector<int>& gts1,
								const vector<int>& gts2, int max_dist) const {
	int	counter1 = 0;	// different genotype
	int	counter2 = 0;	// same genotype
	for(size_t i = 0U; i < gts1.size(); ++i) {
		if(gts1[i] != gts2[i])
			counter1 += 1;
		if(gts1[i] + gts2[i] != 1)
			counter2 += 1;
		if(counter1 > max_dist && counter2 > max_dist)
			return pair<int,bool>(max_dist + 1, false);		// dummy value
	}
	return pair<int,bool>(std::min(counter1, counter2), counter1 > max_dist);
}

vector<VCFHeteroHomo *> VCFHeteroHomo::divide_into_chromosomes() const {
	vector<VCFHeteroHomo *>	vcfs;
	vector<const Map *>		chr_maps = genetic_map.divide_into_chromosomes();
	auto	iter_map = chr_maps.begin();
	string	prev_chr = "";
	vector<VCFHeteroHomoRecord *>	rs;
	for(auto p = records.begin(); p != records.end(); ++p) {
		const string&	chr = (*p)->chrom();
		if(chr != prev_chr) {
			if(!rs.empty()) {
				vcfs.push_back(new VCFHeteroHomo(header, samples,
														rs, **iter_map));
				++iter_map;
				rs.clear();
			}
			prev_chr = chr;
		}
		rs.push_back((*p)->copy());
	}
	vcfs.push_back(new VCFHeteroHomo(header, samples, rs, **iter_map));
	return vcfs;
}

void VCFHeteroHomo::update_genotypes(const std::vector<STRVEC>& GT_table) {
	for(size_t i = 0U; i < records.size(); ++i)
		records[i]->set_GTs(GT_table[i]);
	VCFFamily::update_genotypes(GT_table);
}

// orig_vcfは巨大で何度も読みたくないので、一度読んで全ての家系のVCFを作る
map<pair<VCFHeteroHomo::Parents,bool>, VCFHeteroHomo *>
VCFHeteroHomo::create_vcfs(VCFOriginal *orig_vcf,
							const vector<Parents>& families,
							const PedigreeTable& pedigree,
							const Map& geno_map,
							bool debug) {
	map<pair<Parents,bool>, vector<VCFHeteroHomoRecord *>>	selected_records;
	const auto	family_columns = orig_vcf->collect_family_columns(
														families, pedigree);
	const STRVEC&	orig_samples = orig_vcf->get_samples();
	while(true) {
		const VCFRecord	*record = orig_vcf->next();
		if(record == NULL)
			break;
		// 短縮のため、1染色体のみにする
		if(debug) {
			const auto	pos = orig_vcf->record_position(*record);
			if(pos.first == 2)
				break;
		}
		for(auto p = family_columns.begin(); p != family_columns.end(); ++p) {
			const auto	columns = *p;
			if(columns.size() < 10U)
				continue;
			STRVEC	v(columns.size() + 9);
			STRVEC	samples(columns.size());
			record->copy_properties(v.begin());
			const STRVEC	w = record->gts();
			size_t	i = 0U;
			for(auto p = columns.begin(); p != columns.end(); ++p) {
				v[i+9] = w[*p-9];
				samples[i] = orig_samples[*p-9];
				++i;
			}
			
			Parents	parents(samples[0], samples[1]);
			auto	*new_record = new VCFHeteroHomoRecord(v, samples);
			if(new_record->is_hetero_and_homo(true)) {		// mat is hetero
				const auto	key = pair<Parents,bool>(parents, true);
				selected_records[key].push_back(new_record);
			}
			if(new_record->is_hetero_and_homo(false)) {		// pat is hetero
				const auto	key = pair<Parents,bool>(parents, false);
				selected_records[key].push_back(new_record);
			}
		}
		delete record;
	}
	
	std::map<pair<Parents,bool>, VCFHeteroHomo *> vcfs;
	for(auto p = selected_records.begin(); p != selected_records.end(); ++p) {
		auto	records = p->second;
		const vector<STRVEC>	header = orig_vcf->select_header(records[0]);
		auto	vcf = new VCFHeteroHomo(header, records[0]->get_samples(),
															records, geno_map);
		// 作られたVCFが小さいと染色体ごとなくなってchrsが間違うことがあるので
		orig_vcf->copy_chrs(vcf);
		vcfs[p->first] = vcf;
	}
	return vcfs;
}