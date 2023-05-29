/* TODO launch external window after filters button */
/* TODO put border around entire value entry section, to remove awkward look */
/* TODO filter out special characters from input */
/* TODO add switch below 'set filters' */
/* TODO see where two numbers added messes up total at bottom, and adjust the range
 * of acceptable values in do_add accordingly */
/* TODO make totals BOLD and BIG */
/* TODO less 0s https://docs.gtk.org/gtk3/treeview-tutorial.html#cell-data-functions */
#include <stdlib.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#define _ISOC99_SOURCE 
#include <math.h>
#define MAX_ENTRIES 100000 /* unused */
#define MAX_DATE_CHARS 21 /* adjustable, includes '\0' */ 
#define MAX_METHOD_CHARS 21 /* adjustable, includes '\0' */ 
#define MAX_PERSON_CHARS 101 /* adjustable, includes '\0' */
#define MAX_AMOUNT_CHARS 15 /* double has a precision of 18
			     * so this limits that and also large numbers */
#define FILENAME_SIZE 250 /* adjustable */
#define MAX_YEAR_CHARS 6 /* cannot exceed number of digits of MAX_YEAR_NUMBER + 1 or '\0' */
#define MAX_DAY_CHARS 4 /* cannot exceed number of digits of MAX_DAY_NUMBER + 1 for '\0' */
#define MAX_MONTH_CHARS 4 /* cannot exceed number of digits of MAX_MONTH_NUMBER + 1 for '\0' */
#define MAX_DAY_NUMBER 254 /* cannot exceed UCHAR_MAX, MAX_DAY_CHARS is number of digits of this + 1 for '\0' */
#define MAX_MONTH_NUMBER 254 /* cannot exceed UCHAR_MAX, MAX_MONTH_CHARS is number of digits of this + 1 for '\0' */
#define MAX_YEAR_NUMBER 65534 /* cannot exceed UINT_MAX, MAX_YEAR_CHARS is number of digits of this + 1 for '\0' */

/* enums */
enum exit_codes {SUCCESS,FAILURE,UNFINISHED};
enum status {NONE, START, FINISH};
enum columns {DATE_C, PERSON_C, PAYMENT_METHOD_C, AMOUNT_C, YEAR_C, MONTH_C, DAY_C, SHOW_C, TOTAL_COLUMNS};
enum endianness {IS_BIG_ENDIAN,IS_LITTLE_ENDIAN};

/* Init global variables */
GtkWidget *amount_entry, *date_entry, *person_entry, *method_entry;
GtkTextBuffer *error_buffer;
GtkWidget *error_widget;
GtkWidget *scrolled_window;
GtkWidget *total_filtered_results_label;
GtkWidget *total_results_label;
GtkWidget *window;
double filtered_amount_total;
double amount_total;
char filename[FILENAME_SIZE] = "purchase_log.csv";

/* Functions */
void skip_whitespace(char **text_skip) {
	for(;**text_skip == ' ';(*text_skip)++) {}; 
}

unsigned char
check_endianness(void)
{

        unsigned short a=0x1234;
        if (*((unsigned char *)&a)==0x12)
                return IS_BIG_ENDIAN;
        else
                return IS_LITTLE_ENDIAN;

}

unsigned char validate_day(unsigned long *number) 
{
	if(*number < 0 || *number > MAX_DAY_NUMBER)
		return FAILURE;
	return SUCCESS;
}

unsigned char validate_month(unsigned long *number)
{
	if(*number < 0 || *number > MAX_MONTH_NUMBER)
		return FAILURE;
	return SUCCESS;
}

unsigned char validate_year(unsigned long *number)
{
	if(*number < 0 || *number > MAX_YEAR_NUMBER)
		return FAILURE;
	return SUCCESS;
}

unsigned char validate_amount(char *str, double *number)
{

	char *endptr;

	if (!strlen(str)) {
		gtk_text_buffer_set_text(error_buffer,"Please enter a fee amount",-1);
		return FAILURE;
	}
	if(strlen(str) > MAX_AMOUNT_CHARS - 1)  {
		gtk_text_buffer_set_text(error_buffer,"Too many characters entered for amount",-1);
		return FAILURE;
	}

	*number = strtod(str,&endptr);

	if(endptr == str) {
		gtk_text_buffer_set_text(error_buffer,"Fee amount entered has an invalid format."
				" Please enter ascii digits.  You can include a decimal.",-1);
		return FAILURE;
//	} else if (*number >= HUGE_VAL || *number <= -HUGE_VAL) {
//		gtk_text_buffer_set_text(error_buffer,"Fee amount entered has too many digits",-1);
//		return FAILURE;
	} else if (*number <= -7777777777.55 || *number >= 7777777777.55) {
		gtk_text_buffer_set_text(error_buffer,"Fee amount out of range, should be between"
				" -7777777777.55 and 7777777777.55",-1);
		return FAILURE;

	} else if (*endptr != '\0') {
		skip_whitespace(&endptr);
		if(*endptr != '\0') {
			gtk_text_buffer_set_text(error_buffer,"Only digits in fee amount please",-1);
			return FAILURE;
		}
	}

	return SUCCESS;

}

unsigned char validate_person(char *text)
{
	if(!strlen(text)) {
		fprintf(stderr,"Please enter a person\n");
		gtk_text_buffer_set_text(error_buffer,"Please enter a person",-1);
		return FAILURE;
	}
	if(strlen(text) > MAX_PERSON_CHARS - 1)  {
		fprintf(stderr,"Name of person is too long\n");
		gtk_text_buffer_set_text(error_buffer,"Name of person is too long, "
				"recompile to allow longer person names",-1);
		return FAILURE;
	}
	if(strchr(text,',')) {
		fprintf(stderr,"Customer name cannot contain commas\n");
		gtk_text_buffer_set_text(error_buffer,"Customer name cannot contain commas",-1);
		return FAILURE;
	}
	if(strchr(text,'\n')) {
		fprintf(stderr,"Customer name cannot contain newlines\n");
		gtk_text_buffer_set_text(error_buffer,"Customer name cannot contain newlines",-1);
		return FAILURE;
	}
	return SUCCESS;
}

unsigned char validate_method(char *text)
{
	if(strlen(text) > MAX_METHOD_CHARS - 1)  {
		gtk_text_buffer_set_text(error_buffer,"Name of payment method is too long, "
				"recompile to allow longer method names",-1);
		return FAILURE;
	}
	if(strchr(text,',')) {
		gtk_text_buffer_set_text(error_buffer,"Method name cannot contain commas",-1);
		return FAILURE;
	}
	if(strchr(text,'\n')) {
		gtk_text_buffer_set_text(error_buffer,"Method name cannot contain newlines",-1);
		return FAILURE;
	}
	return SUCCESS;
}
unsigned char validate_date (char *text, unsigned int *year_s, unsigned char *month_s, unsigned char *day_s)
{
	unsigned long number;
	char *endptr;

	/* return failed if nothing in string */
	if(!strlen(text)) {
		gtk_text_buffer_set_text(error_buffer,"Please enter a date",-1);
		return FAILURE;
	}
	if(strlen(text) > MAX_DATE_CHARS - 1) {
		gtk_text_buffer_set_text(error_buffer,"Entered too many characters, must be in dd/mm/yy format",-1);
		return FAILURE;
	}

	/* skip whitespace */
	skip_whitespace(&text);
	if(*text == '\0') {
		gtk_text_buffer_set_text(error_buffer,"0: Invalid date, must be in dd/mm/yy format",-1);
		return FAILURE;
	}
	/* find day */
	number = strtoul(text,&endptr,10);
	if(endptr == text || *endptr != '/' || (validate_day(&number) == FAILURE)) {
		gtk_text_buffer_set_text(error_buffer,"Invalid date, must be in dd/mm/yy format",-1);
		return FAILURE;
	}
	*day_s = number;
	text = endptr + 1;
	/* find month */
	number = strtoul(text,&endptr,10);
	if(endptr == text || *endptr != '/' || (validate_month(&number) == FAILURE)) {
		gtk_text_buffer_set_text(error_buffer,"Invalid date, must be in dd/mm/yy format",-1);
		return FAILURE;
	}
	*month_s = number;
	text = endptr + 1;
	/* find year */
	number = strtoul(text,&endptr,10);
	if(endptr == text || (validate_year(&number) == FAILURE) || !(*endptr == '\0' || *endptr == ' ')) {
		gtk_text_buffer_set_text(error_buffer,"Invalid date, must be in dd/mm/yy format",-1);
		return FAILURE;
	}
	*year_s = number;
	text = endptr;
	/* skip whitespace */
	skip_whitespace(&text);
	if(*text != '\0') {
		gtk_text_buffer_set_text(error_buffer,"Invalid date, must be in dd/mm/yy format",-1);
		return FAILURE;
	}

	return SUCCESS;
}

char * strsep_custom(char **str,char delim) {

        char *save = *str;
        if(!(**str)) {
                return NULL;
        }
        *str = strchr(*str,delim);
        if(!(*str)) {
                return save;
                *str = NULL;
        }
        **str = 0;
        (*str)++;
        return save;
}

unsigned char truncate_double(double *temp_double)
{

	char string_from_double[50] = {0};
	char *endptr;

	snprintf(string_from_double,sizeof(string_from_double),"%.2lf",*temp_double);
	printf("what snprintf produced trying to convert double to string: %s\n",string_from_double);
	*temp_double = strtod(string_from_double,&endptr);
	if(endptr == string_from_double) {
		gtk_label_set_text( GTK_LABEL(total_results_label), "error");
		gtk_label_set_text( GTK_LABEL(total_filtered_results_label), "error");
		gtk_text_buffer_set_text(error_buffer,"snprintf() outputting garbage",-1);
		fprintf(stderr,"can't convert to double from snprintf created string\n" 
				"snprintf() outputting garbage\n");
		return FAILURE;
	}
	return SUCCESS;

}
unsigned char add_all_rows(GtkTreeModel *model)
{
	/* first parameter of this function is accepting our base GtkListStore cast to a GtkTreeModel */
	GtkTreeIter iter;
	gboolean is_visible;
	gdouble gitem_amount;
	char string_from_double[50] = {0};
	double item_amount = 0;
	/* set globally defined totals to 0, because we are re-counting everything in the liststore */
	amount_total = 0;
	filtered_amount_total = 0;

	if(gtk_tree_model_get_iter_first(model,&iter) == FALSE) {
		gtk_label_set_text( GTK_LABEL(total_results_label), "0");
		gtk_label_set_text( GTK_LABEL(total_filtered_results_label), "0");
		return SUCCESS;
	}

	do {
		gtk_tree_model_get(model, &iter, SHOW_C,&is_visible,-1); 
		gtk_tree_model_get(model, &iter, AMOUNT_C, &gitem_amount, -1);
		item_amount = (double) gitem_amount;
		printf("item amount: %lf\n",item_amount);
		/* truncate double the same way we do the rest of the program, so totals make sense */
		/* ie with the %.2lf snprintf/sprintf format specifier */
		if(truncate_double(&item_amount) == FAILURE)
			return FAILURE;
		if(is_visible)
			filtered_amount_total += item_amount;
		amount_total += item_amount;

	} while(gtk_tree_model_iter_next(model,&iter));

	printf("filtered total paid: %lf\n",filtered_amount_total);
	printf("total paid: %lf\n",amount_total);
	snprintf(string_from_double,50,"%.2lf",amount_total);
	gtk_label_set_text(GTK_LABEL(total_results_label), string_from_double);
	snprintf(string_from_double,50,"%.2lf",filtered_amount_total);
	gtk_label_set_text(GTK_LABEL(total_filtered_results_label), string_from_double);

	return SUCCESS;

}

unsigned char load_items(GtkListStore *model)
{

	unsigned long current_line = 0;
	FILE *the_file;
	char * retval;
	size_t line_length;
	char line[MAX_DAY_CHARS + MAX_MONTH_CHARS + MAX_YEAR_CHARS +
		 MAX_METHOD_CHARS + MAX_PERSON_CHARS + MAX_AMOUNT_CHARS + 20];
	char finished_message[FILENAME_SIZE + 35] = {0};
	char error_message[FILENAME_SIZE + 35] = {0};
	char *newline_location;
	char *endptr;
	char *token;
	char *end;
	unsigned long number;
	double amount_s;
	char date_s[MAX_DATE_CHARS] = {0};
	unsigned char day;
	unsigned char month;
	unsigned int year;
	char *person_s;
	char *method_s;

	snprintf(error_message,sizeof(error_message),"Cannot open file at: %s",filename);

	errno = 0;
	the_file = fopen(filename,"r");
	if(!the_file) {
		perror("");
		fprintf(stderr,"%s\n",error_message);
		gtk_text_buffer_set_text(error_buffer,error_message,-1);
	  	return FAILURE;
	}

	snprintf(finished_message,sizeof(finished_message),"Finished loading file at: %s",filename);

	for(;current_line < ULONG_MAX;current_line++) {

		memset(line,0,sizeof(line));
		errno = 0;
		retval = fgets(line,sizeof(line),the_file);
		if(!retval) {
			if(ferror(the_file)) {
				perror("Couldn't fully load file: ");
				gtk_text_buffer_set_text(error_buffer,"Couldn't fully load file, "
						"fatal error, should exit program",-1);
				fclose(the_file);
				return UNFINISHED;
			} else if(feof(the_file)) {
				if(current_line == 0) {
					fprintf(stderr,"Empty file, missing csv header, corrupt file %lu\n",current_line);
					gtk_text_buffer_set_text(error_buffer,"Empty file, missing csv header, corrupt file",-1);
					fclose(the_file);
					return FAILURE;
				} else if (current_line == 1) {
					fprintf(stderr,"File is empty, nothing to do\n");
					gtk_text_buffer_set_text(error_buffer,"Couldn't fully load file, "
							"unknown error, should exit program",-1);
					fclose(the_file);
					return SUCCESS;
				} else {
					//SUCESSS !
					gtk_text_buffer_set_text(error_buffer,finished_message,-1);
					break;
				}
			} else {
				fprintf(stderr,"Couldn't fully load file, unknown error, should exit program\n");
				gtk_text_buffer_set_text(error_buffer,"Couldn't fully load file, "
						"unknown error, should exit program",-1);
				fclose(the_file);
				return UNFINISHED;
			}
		}
		newline_location = strchr(line,'\n');
		if(!newline_location) {
			fprintf(stderr,"No newline at end of line %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,"No newline at end of .csv line."
					"  File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		if(current_line == 0) {
			if(strcmp(line,"Day,Month,Year,Customer,Method,Amount\n")) {
				fprintf(stderr,"Invalid csv header, corrupt file %lu\n",current_line);
				gtk_text_buffer_set_text(error_buffer,"Invalid csv header, corrupt file",-1);
				fclose(the_file);
				return FAILURE;
			}
			continue;
		}
		/* init strsep variable to beginning of scanned line from file */
		end = line;
		/* get day from file ----------------------------------------------------------------- */
		/* run strsetp */
		token = strsep_custom(&end,',');
		/* if no ending comma for day field */
		if(!end) {
			fprintf(stderr,"Missing comma on line %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing comma."
				      	" File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		/* if day csv field is empty, return */
		if(!(*token)) {
			fprintf(stderr,"Missing day on line %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing day."
				      	" File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		number = strtoul(token,&endptr,10);
		if(endptr == token || (validate_day(&number) == FAILURE) || *endptr != '\0') {
			fprintf(stderr,"Missing day on line %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing day."
				      	" File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		day = (unsigned char) number;
		printf("day: %u\n",day);
		/* get month from file -------------------------------------------------------------------- */
		token = strsep_custom(&end,',');
		/* if null character after last valid csv field, return */
		if(!token) {
			fprintf(stderr,".csv ended prematurely on line %lu, file is corrupted\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing comma."
				      	" File is corrupted, please exit program and check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		/* if no ending comma for month field */
		if(!end) {
			fprintf(stderr,"Missing comma on line %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing comma."
				      	" File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		/* if month csv field is empty, return */
		if(!(*token)) {
			fprintf(stderr,"Missing month on line %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing month."
				      	" File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		number = strtoul(token,&endptr,10);
		if(endptr == token || (validate_month(&number) == FAILURE) || *endptr != '\0') {
			fprintf(stderr,"Missing month on line %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing month."
				      	" File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		month = (unsigned char) number;
		printf("month: %u\n",month);
		/* get year from file --------------------------------------------------------------------------- */
		token = strsep_custom(&end,',');
		/* if null character after last valid csv field, return */
		if(!token) {
			fprintf(stderr,".csv ended prematurely on line %lu, file is corrupted\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing comma."
				      	" File is corrupted, please exit program and check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		/* if no ending comma for year field */
		if(!end) {
			fprintf(stderr,"Missing comma on line %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing comma."
				      	" File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		/* if year csv field is empty, return */
		if(!(*token)) {
			fprintf(stderr,"Missing year on line %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing year."
				      	" File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		number = strtoul(token,&endptr,10);
		if(endptr == token || (validate_year(&number) == FAILURE) || *endptr != '\0') {
			fprintf(stderr,"Missing year on line %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing year."
				      	" File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		year = (unsigned int) number;
		printf("year: %u\n",year);
		/* get person from file --------------------------------------------------------------- */
		token = strsep_custom(&end,',');
		/* if null character after last valid csv field, return */
		if(!token) {
			fprintf(stderr,".csv ended prematurely on line %lu, file is corrupted\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing comma."
				      	" File is corrupted, please exit program and check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		/* if no ending comma for person field */
		if(!end) {
			fprintf(stderr,"Missing comma on line %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing comma."
				      	" File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		/* if empty person csv field, or person string doesn't validate, return */
		if(validate_person(token) == FAILURE) {
			fprintf(stderr,"Cannot parse person on %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing properly formatted person."
				      	" File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		person_s = token;
		printf("person: %s\n",person_s);
		/* get method from file ------------------------------------------ */ 
		token = strsep_custom(&end,',');
		/* if null character after last valid csv field, return */
		if(!token) {
			fprintf(stderr,".csv ended prematurely on line %lu, file is corrupted\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing comma."
				      	" File is corrupted, please exit program and check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		/* if no ending comma for method field */
		if(!end) {
			fprintf(stderr,"Missing comma on line %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing comma."
				      	" File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		/* method field is optional */
		/* if method is non empty, ie there isn't ,, in csv, then validate it for sanity */
		if(*token) {
			if(validate_method(token) == FAILURE) {
				fprintf(stderr,"Cannot parse person on %lu\n",current_line);
				gtk_text_buffer_set_text(error_buffer,".csv file is missing properly formatted person."
						"File is corrupted, please check file",-1);
				fclose(the_file);
				return UNFINISHED;
			}
			method_s = token;
		} else {/* else just keep method an empty string and put an empty string in liststore */
			method_s = "";
		}
		printf("method: %s\n",method_s);
		/* get amount from file -------------------------------------------------------------------- */
		token = strsep_custom(&end,'\n');
		/* if null character after last valid csv field, return */
		if(!token) {
			fprintf(stderr,".csv ended prematurely on line %lu, file is corrupted\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing comma."
				      	" File is corrupted, please exit program and check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		/* if no ending newline for amount field */
		if(!end) {
			fprintf(stderr,"Missing comma on line %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing comma."
				      	" File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		/* if amount csv field is empty, return */
		if(!(*token)) {
			fprintf(stderr,"Missing month on line %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing month."
				      	" File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		/* if empty person csv field, or amount string doesn't validate, return */
		if(validate_amount(token,&amount_s) == FAILURE) {
			fprintf(stderr,"Cannot parse person on %lu\n",current_line);
			gtk_text_buffer_set_text(error_buffer,".csv file is missing properly formatted person."
				      	"File is corrupted, please check file",-1);
			fclose(the_file);
			return UNFINISHED;
		}
		printf("amount: %lf\n",amount_s);

		snprintf(date_s,sizeof(date_s),"%u/%u/%u",day,month,year);

		gtk_list_store_insert_with_values(model, NULL, -1,
						DATE_C, date_s,
						PERSON_C, person_s,
						PAYMENT_METHOD_C, method_s,
						AMOUNT_C, amount_s,
						YEAR_C, year,
						MONTH_C, month,
						DAY_C, day,
						SHOW_C, 1, /* 1 for, yes show in tree */
						-1);
		printf("processed a line of csv successfully\n");

	} /* end of for loop reading file */

	fclose(the_file);
	printf("success in parsing entire file! :)\n");
	add_all_rows(GTK_TREE_MODEL(model));
	return SUCCESS;

}

/* TODO check error on library functions and handle accordingly */
void save_items(GtkWidget *widget, gpointer model_void) {
	GtkTreeModel *model = (GtkTreeModel *)model_void;
	GtkTreeIter iter;
	char save_message[FILENAME_SIZE + 20] = {0};
	char error_message[FILENAME_SIZE + 40] = {0};
	/*year*/
	guint gyear_s;
	unsigned int year_s;
	/*month*/
	guchar gmonth_s;
	unsigned char month_s;
	/*day*/
	guchar gday_s;
	unsigned char day_s;
	/*name*/
	gchar *gname_s;
	char name_s[MAX_PERSON_CHARS] = {0};
	/*method*/
	gchar *gmethod_s;
	char method_s[MAX_METHOD_CHARS] = {0};
	/*amount*/
	gdouble gitem_amount;
	double item_amount;
	FILE *the_file;

	snprintf(error_message,sizeof(error_message),"Cannot create or open file at %s",filename);

	errno = 0;
	the_file = fopen(filename,"w+");
	if(!the_file) {
		perror("");
		fprintf(stderr,"%s\n",error_message);
		gtk_text_buffer_set_text(error_buffer,error_message,-1);
	  	return;
	}
	snprintf(save_message,sizeof(save_message),"File saved at %s",filename);
	fprintf(the_file,"Day,Month,Year,Customer,Method,Amount\n");

	if(gtk_tree_model_get_iter_first(model,&iter) == FALSE) {
		gtk_text_buffer_set_text(error_buffer,save_message,-1);
		fclose(the_file);
		return;
	}

	do {
		/*year*/
		gtk_tree_model_get(model, &iter, YEAR_C, &gyear_s, -1);
		year_s = (unsigned int) gyear_s;
		/*month*/
		gtk_tree_model_get(model, &iter, MONTH_C, &gmonth_s, -1);
		month_s = (unsigned char) gmonth_s;
		/*day*/
		gtk_tree_model_get(model, &iter, DAY_C, &gday_s, -1);
		day_s = (unsigned char) gday_s;
		/*name*/
		gtk_tree_model_get(model, &iter, PERSON_C, &gname_s, -1);
		snprintf(name_s,sizeof(name_s),"%s",(char *)gname_s);
		g_free(gname_s);
		/*method*/
		gtk_tree_model_get(model, &iter, PAYMENT_METHOD_C, &gmethod_s, -1);
		snprintf(method_s,sizeof(method_s),"%s",(char *)gmethod_s);
		g_free(gmethod_s);
		/*amount*/
		gtk_tree_model_get(model, &iter, AMOUNT_C, &gitem_amount, -1);
		item_amount = (double) gitem_amount;

		fprintf(the_file,"%u,%u,%u,%s,%s,%.2lf\n",day_s,month_s,year_s,name_s,method_s,item_amount);

	} while(gtk_tree_model_iter_next(model,&iter));


	fclose(the_file);
	gtk_text_buffer_set_text(error_buffer,save_message,-1);

}

gboolean keypress_function(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	/*
	if (event->keyval == GDK_KEY_space){
		printf("test!\n");
		return TRUE;
	}
	*/
	return FALSE;
}

void scroll_to_end (GtkWidget *widget, GdkRectangle *allocate, gpointer user_data)
{
	GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment (
	GTK_SCROLLED_WINDOW (scrolled_window));
	double upper = gtk_adjustment_get_upper (adjustment);
	double page_size = gtk_adjustment_get_page_size (adjustment);
	gtk_adjustment_set_value (adjustment, upper - page_size - 0);
}

void do_add(GtkWidget *widget, gpointer model)
{

	double number;
	/* The data at these pointers can't be modified */
	/* ideally we should copy this into a local array
	 * Check to see if it is not undefined behaviour to
	 * pass a local char array to gtk_list_stre_insert_with_value
	 * and if not, then do that, because it's safer and can cut off
	 * trailing whitespace etc */
	char *date_ptr = (char *)gtk_entry_get_text(GTK_ENTRY(date_entry));
	char *person_ptr = (char *)gtk_entry_get_text(GTK_ENTRY(person_entry));
	char *amount_ptr = (char *)gtk_entry_get_text(GTK_ENTRY(amount_entry));
	char *method_ptr = (char *)gtk_entry_get_text(GTK_ENTRY(method_entry));
	unsigned int year_s;
	unsigned char month_s;
	unsigned char day_s;

	if(validate_date(date_ptr,&year_s,&month_s,&day_s) == FAILURE) {
		return;
	}
	if(validate_person(person_ptr) == FAILURE) {
		return;
	}
	if(validate_method(method_ptr) == FAILURE) {
		return;
	}
	if(validate_amount(amount_ptr,&number) == FAILURE)
		return;
	printf("added number: %lf\n",amount_total+number);
	if(amount_total + number >= 7777777777.55 || amount_total + number <= -7777777777.55) {
		gtk_text_buffer_set_text(error_buffer,"Cannot add amount. Amount total out of range, should be between"
				" -7777777777.55 and 7777777777.55",-1);
		return;
	}

	gtk_list_store_insert_with_values(model, NULL, -1,
					DATE_C, date_ptr,
					PERSON_C, person_ptr,
					PAYMENT_METHOD_C, method_ptr,
					AMOUNT_C, number,
					YEAR_C, year_s,
					MONTH_C, month_s,
					DAY_C, day_s,
					SHOW_C, 1, /* 1 for, yes show in tree */
					-1);

	gtk_text_buffer_set_text(error_buffer," ",-1);
	gtk_entry_set_text(GTK_ENTRY(date_entry), "");
	gtk_entry_set_text(GTK_ENTRY(person_entry), "");
	gtk_entry_set_text(GTK_ENTRY(amount_entry), "");
	gtk_entry_set_text(GTK_ENTRY(method_entry), "");

	add_all_rows(GTK_TREE_MODEL(model));

}

/* Main */
int main(int argc, char **argv)
{

	/* Init local variables */
	GtkWidget *grid, *grid2, *box, *box2, *date_label, *person_label, 
		  *amount_label, *add_label, *tree_view;
	GtkWidget *filter_button, *search_button, *edit_button, *add_button, 
		*hide_button, *delete_button, *hide_all_button, *show_all_button, *save_button,
			*method_label;
	GtkTreeViewColumn *column, *column1, *column2, *column3;
	GtkListStore *model;
	GtkTreeModel *filter;

	printf("size of double: %zu\nsize of gdouble %zu\nsize of int: %zu\n"
			"size of unsigned int: %zu\nsize of long: %zu\n"
			"size of unsigned long %zu\n"
			,sizeof(double),sizeof(gdouble),sizeof(int),sizeof(unsigned int)
			,sizeof(long),sizeof(unsigned long));


	/* Init GTK */
	gtk_init(&argc,&argv);
	
	/* Temp debug info */
	printf("Double has a precision of %d digits\n",LDBL_DIG);
	printf("Columns: %d\n",TOTAL_COLUMNS);

	/* Create vertically oriented box to pack program widgets into */
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL,20);

	/* Create window, set title, border width, and size */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW(window) , "Fee Adder" );
	gtk_container_set_border_width (GTK_CONTAINER(window),10); 
	gtk_widget_add_events(window, GDK_KEY_PRESS_MASK);
	gtk_window_set_default_size ( GTK_WINDOW(window), 800, 580);

	/* Destroy window on close */
	g_signal_connect(window,"destroy",G_CALLBACK(gtk_main_quit),NULL);

	/* Create grid for value entry */
	grid = gtk_grid_new();
	gtk_grid_set_row_spacing (GTK_GRID(grid),2);
	gtk_grid_set_column_spacing (GTK_GRID(grid),10);
	/* Add grid to vertical box */
	gtk_box_pack_start (GTK_BOX (box), grid, FALSE, TRUE, 0);
	/* Add margin to value entry grid */
	gtk_widget_set_margin_top(grid,15);

	/* Add date entry to value entry grid */
	date_label = gtk_label_new("Date (dd/mm/yy)");
        gtk_widget_set_halign (date_label, GTK_ALIGN_START);
	gtk_widget_set_margin_start(date_label,3);
	gtk_grid_attach(GTK_GRID(grid),date_label,0,0,1,1);
	date_entry = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(grid),date_entry,0,1,1,1);
	gtk_entry_set_width_chars (GTK_ENTRY(date_entry), 8);

	/* Add person entry to value entry grid */
	person_label = gtk_label_new("Customer");
        gtk_widget_set_halign (person_label, GTK_ALIGN_START);
	gtk_widget_set_margin_start(person_label,3);
	gtk_grid_attach(GTK_GRID(grid),person_label,1,0,1,1);
	person_entry = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(grid),person_entry,1,1,1,1);
	gtk_entry_set_width_chars (GTK_ENTRY(person_entry), 20);

	/* Add payment method to value entry grid */
	method_label = gtk_label_new("Method");
        gtk_widget_set_halign (method_label, GTK_ALIGN_START);
	gtk_widget_set_margin_start(method_label,3);
	gtk_grid_attach(GTK_GRID(grid),method_label,2,0,1,1);
	method_entry = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(grid),method_entry,2,1,1,1);
	gtk_entry_set_width_chars (GTK_ENTRY(method_entry), 10);

	/* Add amount entry to value entry grid */
	amount_label = gtk_label_new("Shekels");
        gtk_widget_set_halign (amount_label, GTK_ALIGN_START);
	gtk_widget_set_margin_start(amount_label,3);
	gtk_grid_attach(GTK_GRID(grid),amount_label,3,0,1,1);
	amount_entry = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(grid),amount_entry,3,1,1,1);
	gtk_entry_set_width_chars (GTK_ENTRY(amount_entry), 10);
	/* Align value entry grid */
        gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_end(grid,70);

	/* Add 'add' button to value entry grid */
	add_label = gtk_label_new("");
	gtk_grid_attach(GTK_GRID(grid),add_label,4,0,1,1);
	add_button = gtk_button_new_with_label("Add");
	gtk_grid_attach(GTK_GRID(grid), add_button, 4, 1, 1, 1);

	/* Create horizontally oriented box to horizontally pack progam widgets into */
	box2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,20);
	/* Pack horizontal box into vertical box */
	gtk_box_pack_start (GTK_BOX (box), box2, FALSE, TRUE, 0);
	/* Align horizontal box */
        gtk_widget_set_halign (box2, GTK_ALIGN_CENTER);

	/* Create another vertically oriented box to pack into last hbox
	 * this will contain the scrolled window and totals */
	GtkWidget *box3 = gtk_box_new(GTK_ORIENTATION_VERTICAL,20);
	gtk_box_pack_start (GTK_BOX (box2), box3, FALSE, TRUE, 0);

	/* Create a scrollable window */
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	/* Set preferred size of scrollable window */
 	gtk_widget_set_size_request (scrolled_window, 540, 300);
	/* Always include a vertical scroll cursor on scroll window */
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	/* Add scrollable window to horizontal box */
	gtk_box_pack_start (GTK_BOX (box3), scrolled_window, TRUE, TRUE, 0);
	/* Align scrollable window */
        gtk_widget_set_halign (scrolled_window, GTK_ALIGN_START);
	g_object_set (scrolled_window, "vexpand", TRUE, NULL);
	g_object_set (scrolled_window, "hexpand", FALSE, NULL);
        //gtk_widget_set_valign (scrolled_window, GTK_ALIGN_START);
        //g_object_set (scrolled_window, "margin", 10, NULL);
    	gtk_widget_show (scrolled_window);

	/* Create liststore for table */
	model = gtk_list_store_new(
		TOTAL_COLUMNS,  /* required parameter, total columns */
		G_TYPE_STRING,  /* 0th column, date, DATE_C */
		G_TYPE_STRING,  /* first column, person, PERSON_C */
		G_TYPE_STRING,  /* second column, payment method, PAYMENT_METHOD_C */
		G_TYPE_DOUBLE,  /* third column, amount, AMOUNT_C */
		G_TYPE_UINT,   /* fourth column, year, YEAR_C */
		G_TYPE_UCHAR,   /* fifth column, month, MONTH_C */
		G_TYPE_UCHAR,   /* sixth column, day, DAY_C */
		G_TYPE_BOOLEAN  /* 1 for show, 0 for hide */
		);

	/* Example rows, if any */
	/*
	gtk_list_store_insert_with_values(model, NULL, -1,
		DATE_C, "03/22/23",
		PERSON_C, "Natalie Smith",
		AMOUNT_C, 100.20,
		-1);
	*/

	/* create treestore as filter of liststore */
	filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(model),NULL);
	gtk_tree_model_filter_set_visible_column(GTK_TREE_MODEL_FILTER(filter),TOTAL_COLUMNS - 1);

	/* Create treeview from treestore */
	tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(filter));

	/* Scroll the scrollable window to bottom when the size of treeview changes
	 * Default behaviour is to hide new rows at bottom for some reason */
	g_signal_connect (tree_view, "size-allocate", G_CALLBACK (scroll_to_end), NULL);

	/* Unref treestore */
	g_object_unref(filter);

	/* Add columns to treeview */
	/* Treeview column 0 */
	column = gtk_tree_view_column_new_with_attributes("Date paid",
		gtk_cell_renderer_spin_new(),
		"text", DATE_C,
		NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
	gtk_tree_view_column_set_expand ( column, TRUE);
	gtk_tree_view_column_set_fixed_width (column,125);

	/* Treeview column 1 */
	column1 = gtk_tree_view_column_new_with_attributes("Customer",
		gtk_cell_renderer_text_new(),
		"text", PERSON_C,
		NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column1);
	gtk_tree_view_column_set_expand ( column1, TRUE);
	gtk_tree_view_column_set_fixed_width (column1,190);

	/* Treeview column 2 */
	column2 = gtk_tree_view_column_new_with_attributes("Method",
		gtk_cell_renderer_text_new(),
		"text", PAYMENT_METHOD_C,
		NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column2);
	gtk_tree_view_column_set_expand ( column2, TRUE);
	gtk_tree_view_column_set_fixed_width (column2,105);


	/* Treeview column 3 */
	column3 = gtk_tree_view_column_new_with_attributes("Shekels",
		gtk_cell_renderer_text_new(),
		"text", AMOUNT_C,
		NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column3);
	gtk_tree_view_column_set_expand ( column3, TRUE);
	gtk_tree_view_column_set_fixed_width (column3,100);


	/* Create grid for right-side buttons */
	grid2 = gtk_grid_new();
	gtk_grid_set_row_spacing (GTK_GRID(grid2),20);
	gtk_widget_set_margin_top(grid2,55);

	/* Create filter button */
	filter_button = gtk_button_new_with_label("Set filters");
	gtk_grid_attach(GTK_GRID(grid2), filter_button, 0, 0, 1, 1);

	/* Create search button */
	search_button = gtk_button_new_with_label("Show all");
	gtk_grid_attach(GTK_GRID(grid2), search_button, 0, 1, 1, 1);

	/* Create edit button */
	edit_button = gtk_button_new_with_label("Edit row");
	gtk_grid_attach(GTK_GRID(grid2), edit_button, 0, 2, 1, 1);

	/* Create delete button */
	delete_button = gtk_button_new_with_label("Delete row");
	gtk_grid_attach(GTK_GRID(grid2), delete_button, 0, 3, 1, 1);

	/* Create hide button */
	hide_button = gtk_button_new_with_label("Hide");
	//gtk_grid_attach(GTK_GRID(grid2), hide_button, 0, 4, 1, 1);

	/* Create Hide all button */
	hide_all_button = gtk_button_new_with_label("Hide all");
	//gtk_grid_attach(GTK_GRID(grid2), hide_all_button, 0, 5, 1, 1);

	/* Create Show all button */
	show_all_button = gtk_button_new_with_label("Show all");
	//gtk_grid_attach(GTK_GRID(grid2), show_all_button, 0, 6, 1, 1);

	/* Create Save button */
	save_button = gtk_button_new_with_label("Save");
	gtk_grid_attach(GTK_GRID(grid2), save_button, 0, 7, 1, 1);

	/* Pack grid2 to box2 */
	gtk_box_pack_start (GTK_BOX (box2), grid2, TRUE, TRUE, 0);

	/* Align grid2 */
        gtk_widget_set_halign (grid2, GTK_ALIGN_START);
        gtk_widget_set_valign (grid2, GTK_ALIGN_START);

	/* create seperator for before totals */
	GtkWidget *before_totals_seperator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
 	gtk_widget_set_size_request (before_totals_seperator, 40, 2);

	/* Create grid for totals */
	GtkWidget *grid3, *filters_applied_label, *filters_applied_results_label, *total_filtered_label,
		  *total_label;
	grid3 = gtk_grid_new();
	/* Set grid spacing */
	gtk_grid_set_row_spacing (GTK_GRID(grid3),9);
	gtk_grid_set_column_spacing (GTK_GRID(grid3),0);
	/* Set grid alignment */
        gtk_widget_set_halign (grid3, GTK_ALIGN_END);
	gtk_widget_set_margin_end(grid3,30);
	/* Create grid members */
	//filters_applied_label = gtk_label_new("Filters applied: ");
        //gtk_widget_set_halign (filters_applied_label, GTK_ALIGN_START);
	//filters_applied_results_label = gtk_label_new("date, payment method");
        //gtk_widget_set_halign (filters_applied_results_label, GTK_ALIGN_START);
	total_filtered_label = gtk_label_new("Total paid including filters:  ₪");
        gtk_widget_set_halign (total_filtered_label, GTK_ALIGN_END);
	total_filtered_results_label = gtk_label_new("0.00");
        gtk_widget_set_halign (total_filtered_results_label, GTK_ALIGN_END);
	total_label = gtk_label_new("Total paid:  ₪");
        gtk_widget_set_halign (total_label, GTK_ALIGN_END);
	total_results_label = gtk_label_new("0.00");
        gtk_widget_set_halign (total_results_label, GTK_ALIGN_END);
	/* Attach grid members */
	//gtk_grid_attach(GTK_GRID(grid3),filters_applied_label,0,0,1,1);
	//gtk_grid_attach(GTK_GRID(grid3),filters_applied_results_label,1,0,3,1);
	gtk_grid_attach(GTK_GRID(grid3),before_totals_seperator,0,0,2,1);
	gtk_grid_attach(GTK_GRID(grid3),total_filtered_label,0,1,1,1);
	gtk_grid_attach(GTK_GRID(grid3),total_filtered_results_label,1,1,1,1);
	gtk_grid_attach(GTK_GRID(grid3),total_label,0,2,1,1);
	gtk_grid_attach(GTK_GRID(grid3),total_results_label,1,2,1,1);
	gtk_box_pack_start (GTK_BOX (box3), grid3, TRUE, TRUE, 0);

	/* error widget */
	error_buffer = gtk_text_buffer_new(NULL);
	error_widget = gtk_text_view_new_with_buffer(error_buffer);
	gtk_text_buffer_set_text(error_buffer," ",-1);
	gtk_box_pack_start (GTK_BOX (box), error_widget, 0, 0, 0);

	printf("Fee adder 0.1\n");
	
	if(check_endianness() == IS_BIG_ENDIAN) {
		gtk_text_buffer_set_text(error_buffer,"Computer should be litte endian, please exit to avoid data issues\n",-1);	
		fprintf(stderr,"Computer should be little endian, please exit to avoid data issues\n");	

	}
	if (sizeof(void *) != 8) {
		gtk_text_buffer_set_text(error_buffer,"Error: please use a 64-bit computer, please exit to avoid data issues\n",-1);
		fprintf(stderr,"Error: please use a 64-bit computer, please exit to avoid data issues\n");
        }
	/* warn if compiler didn't create correct length for types */
	if(sizeof(double) != 8) {
		gtk_text_buffer_set_text(error_buffer,"size of double must equal 8, please exit to avoid data issues\n",-1);	
		fprintf(stderr,"size of double must equal 8, please exit to avoid data issues\n");	
	}
	if(sizeof(gdouble) != 8) {
		gtk_text_buffer_set_text(error_buffer,"size of gdouble must equal 8, please exit to avoid data issues\n",-1);	
		fprintf(stderr,"size of gdouble must equal 8, please exit to avoid data issues\n");	
	}
	if(sizeof(unsigned int) != 4) {
		gtk_text_buffer_set_text(error_buffer,"size of unsigned int must equal 4, please exit to avoid data issues\n",-1);	
		fprintf(stderr,"size of unsigned int must equal 4, please exit to avoid data issues\n");	
	}
	if(sizeof(int) != 4) {
		gtk_text_buffer_set_text(error_buffer,"size of int must equal 4, please exit to avoid data issues\n",-1);	
		fprintf(stderr,"size of int must equal 4, please exit to avoid data issues\n");	
	}
	if(sizeof(long) != 8) {
		gtk_text_buffer_set_text(error_buffer,"size of long must equal 8, please exit to avoid data issues\n",-1);
		fprintf(stderr,"size of long must equal 8, please exit to avoid data issues\n");
	}
	if(sizeof(unsigned long) != 8) {
		gtk_text_buffer_set_text(error_buffer,"size of unsigned long must equal 8, please exit to avoid data issues\n",-1);	
		fprintf(stderr,"size of unsigned long must equal 8, please exit to avoid data issues\n");	
	}
	/* load items from csv file into liststore, treestore, and treeview */
	load_items(model);

	/* After clicking add, call 'do_add' function */
	g_signal_connect(add_button,"clicked",G_CALLBACK(do_add),model);
	/* After clicking save, call 'save_items */
	g_signal_connect(save_button,"clicked",G_CALLBACK(save_items),GTK_TREE_MODEL(model));
	/* After pressing a keyboard button, called 'key_press_event */
	g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (keypress_function), NULL);

	/* Add tree view to scrolled window */
	gtk_container_add(GTK_CONTAINER(scrolled_window),tree_view);	
	
	/* Add box to window */
	gtk_container_add(GTK_CONTAINER(window),box);

	/* Show everything */
	gtk_widget_show_all(box);
	gtk_widget_show_all(scrolled_window);
	gtk_widget_show_all(window);
	gtk_main();

	return 0;
}