#include <iostream>
#include <cassert>
#include "../include/graphite.h"
#include "../include/SampleManager.h"
#include "../include/LargeFamily.h"
#include "../include/SmallFamily.h"
#include "../include/VCFHeteroHomoPP.h"
#include "../include/VCFOneParentPhased.h"
#include "../include/VCFProgenyPhased.h"
#include "../include/VCFIsolated.h"
#include "../include/option.h"
#include "../include/common.h"

using namespace std;


//////////////////// Materials ////////////////////

Materials::Materials(const string& path, const Map *m) : path_map(path),
										geno_map(m),
										chr_maps(Map::create_chr_maps(m)) { }

Materials::~Materials() {
	delete geno_map;
	Common::delete_all(chr_maps);
}

const Map *Materials::get_chr_map(int i) const {
	if(geno_map->is_empty())
		return chr_maps[0];
	else
		return chr_maps[i];
}

double Materials::total_cM() const {
	double	length = 0.0;
	for(auto p = chr_maps.begin(); p != chr_maps.end(); ++p)
		length += (*p)->total_cM();
	return length;
}

void Materials::display_map_info() const {
	cerr << "Genetic Map : ";
	if(geno_map->is_empty()) {
		cerr << "default map(1Mbp=1cM)." << endl;
	}
	else {
		cerr << path_map << endl;
		cerr << chr_maps.size() << " chrmosomes "
								<< total_cM() << " cM." << endl;
	}
}

Materials *Materials::create(const Option *option) {
	const auto	*geno_map = Map::read(option->path_map);
	return new Materials(option->path_map, geno_map);
}


//////////////////// process ////////////////////

void display_chromosome_info(const VCFSmall *orig_vcf) {
	cerr << "chr : " << orig_vcf->get_records().front()->chrom() << endl;
	if(orig_vcf->size() == 1) {
		cerr << "1 record." << endl;
	}
	else {
		cerr << orig_vcf->size() << " records." << endl;
	}
}

VCFSmall *impute_vcf_chr(const VCFSmall *orig_vcf, SampleManager *sample_man,
									const Map& geno_map, const Option *option) {
	display_chromosome_info(orig_vcf);
	
	const auto	large_families = sample_man->get_large_families();
	auto	merged_vcf = LargeFamily::correct_large_family_VCFs(
									orig_vcf, large_families, geno_map, option);
	if(option->only_large_families)
		return merged_vcf;
	sample_man->add_imputed_samples(merged_vcf->get_samples());
	
	merged_vcf = SmallFamily::impute_small_family_VCFs(orig_vcf, merged_vcf,
														geno_map, sample_man,
														option->num_threads);
	
	// At last, impute isolated samples
	const STRVEC	samples = sample_man->extract_isolated_samples();
	if(!samples.empty()) {
		VCFSmall	*new_imputed_vcf = SmallFamily::impute_iolated_samples(
												orig_vcf,
												merged_vcf, sample_man, samples,
												geno_map, option->num_threads);
		vector<VCFSmallBase *>	vcfs{ merged_vcf, new_imputed_vcf };
		VCFSmall	*vcf = merged_vcf;
		merged_vcf = VCFSmall::join(merged_vcf, new_imputed_vcf,
												orig_vcf->get_samples());
		delete vcf;
		delete new_imputed_vcf;
	}
	
	sample_man->clear_imputed_samples();
	return merged_vcf;
}

void print_info(const Option *option) {
	cerr << "input VCF : " << option->path_vcf << endl;
	cerr << "pedigree : " << option->path_ped << endl;
	cerr << "output VCF : " << option->path_out << endl;
}

void impute_VCF(const Option *option) {
	print_info(option);
	Materials	*materials = Materials::create(option);
	materials->display_map_info();
	
	VCFHuge	*vcf = VCFHuge::read(option->path_vcf);
	SampleManager	*sample_man = SampleManager::create(
										option->path_ped, vcf->get_samples(),
										option->lower_progs, option->families);
	sample_man->display_info();
	
	// process chromosome by chromosome
	VCFHuge::ChromDivisor	divisor(vcf);
	bool	first_chromosome = true;
	for(int	chrom_index = 0; ; ++chrom_index) {
		// chrom_index is required only for because they can skip chromosomes
		VCFSmall	*vcf_chrom = divisor.next();
		if(vcf_chrom == NULL)
			break;
		else if(!option->is_efficient_chrom(chrom_index)) {
			delete vcf_chrom;
			continue;
		}
		
		const Map	*gmap = materials->get_chr_map(chrom_index);
		const VCFSmall	*vcf_imputed = impute_vcf_chr(vcf_chrom, sample_man,
																*gmap, option);
		delete vcf_chrom;
		if(first_chromosome) {
			ofstream	ofs(option->path_out);
			vcf_imputed->write(ofs, true);	// write header
		}
		else {
			ofstream	ofs(option->path_out, ios_base::app);
			vcf_imputed->write(ofs, false);
		}
		first_chromosome = false;
		delete vcf_imputed;
	}
	
	delete vcf;
	delete sample_man;
	delete materials;
}

int main(int argc, char **argv) {
	const Option	*option = Option::create(argc, argv);
	if(option == NULL) {
		Option::usage(argv);
		exit(1);
	}
	
	impute_VCF(option);
	delete option;
}
