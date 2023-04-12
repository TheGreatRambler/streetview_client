openMVG_main_SfMInit_ImageListing -i tiles -o matches -c 7 -f 1
openMVG_main_ComputeFeatures -i matches/sfm_data.json -o matches -m SIFT -p HIGH
openMVG_main_ComputeMatches -i matches/sfm_data.json -o matches/matches_putative.bin -p matches/matching_pairs.txt -n HNSWL1
openMVG_main_GeometricFilter -i matches/sfm_data.json -o matches/matches_refined.bin -m matches/matches_putative.bin -g a 
openMVG_main_SfM -i matches/sfm_data.json -M matches/matches_refined.bin -o recon -s INCREMENTAL
openMVG_main_sfmViewer -i recon/sfm_data.bin