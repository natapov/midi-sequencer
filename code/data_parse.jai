#import "Basic";
#import "String";
#import "File";
#import "File_Utilities";
file_name :: "music-scales.csv";

start :: #string DONE
typedef struct Scale_data{
	char* name;
	short notes;
	int number_of_notes;
	short is_matching = 0;
}Scale_data;

const int NUM_SCALES = 243;

Scale_data scale[243] = {
DONE

main :: () {
	builder: String_Builder;
	init_string_builder(*builder);
	defer free(*builder);
	data, success := read_entire_file(file_name);
	defer free(data);
	if !success {
		print("Couldn't find %\n", file_name);
		exit(0);
	}
	format_int := *context.print_style.default_format_int;
	append(*builder, start);
	line := split(data,"\n");
	 
	for i: 0..line.count - 1{
		data := split(line[i],",");
		scale := 0;
		for j: 1..data.count -1{
			note := 1 << (string_to_int(data[j]) );
			scale |= note;
		}
		format_int.base = 2;
		print_to_builder(*builder, "{%,\t0b%,\t", data[0], scale);
		format_int.base = 10;
		print_to_builder(*builder, "%},\n", data.count-2);
	}
	append(*builder, "};");
	success = write_entire_file("scales.h", builder_to_string(*builder));
	if !success print("write failed");

	print("done parsing");
}
