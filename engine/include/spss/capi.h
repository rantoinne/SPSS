#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef struct SPSS_Dataset SPSS_Dataset;
typedef struct SPSS_Output SPSS_Output;

SPSS_Dataset* spss_dataset_create(const char* name);
void spss_dataset_destroy(SPSS_Dataset* ds);
SPSS_Dataset* spss_dataset_clone(const SPSS_Dataset* ds);

const char* spss_dataset_name(const SPSS_Dataset* ds);
size_t spss_dataset_case_count(const SPSS_Dataset* ds);
size_t spss_dataset_var_count(const SPSS_Dataset* ds);

int spss_dataset_add_numeric(SPSS_Dataset* ds, const char* name, int width, int decimals);
int spss_dataset_add_string(SPSS_Dataset* ds, const char* name, int width);
int spss_dataset_add_cases(SPSS_Dataset* ds, size_t n);

double spss_get_numeric(const SPSS_Dataset* ds, size_t row, size_t col);
void spss_set_numeric(SPSS_Dataset* ds, size_t row, size_t col, double value);
const char* spss_get_string(const SPSS_Dataset* ds, size_t row, size_t col);
void spss_set_string(SPSS_Dataset* ds, size_t row, size_t col, const char* value);

const char* spss_var_name(const SPSS_Dataset* ds, size_t col);
const char* spss_var_label(const SPSS_Dataset* ds, size_t col);
void spss_set_var_label(SPSS_Dataset* ds, size_t col, const char* label);
int spss_var_type(const SPSS_Dataset* ds, size_t col); // 0 numeric 1 string
int spss_var_measure(const SPSS_Dataset* ds, size_t col);
void spss_set_var_measure(SPSS_Dataset* ds, size_t col, int measure);

void spss_push_undo(SPSS_Dataset* ds);
int spss_undo(SPSS_Dataset* ds);
int spss_redo(SPSS_Dataset* ds);

int spss_read_csv(SPSS_Dataset** out, const char* path, int header);
int spss_write_csv(const SPSS_Dataset* ds, const char* path);
int spss_read_sav(SPSS_Dataset** out, const char* path);
int spss_write_sav(const SPSS_Dataset* ds, const char* path);

int spss_compute(SPSS_Dataset* ds, const char* target, const char* expression);
int spss_recode_value(SPSS_Dataset* ds, const char* source, const char* target,
                      double from, double to);
int spss_sort(SPSS_Dataset* ds, const char* variable, int ascending);
int spss_weight(SPSS_Dataset* ds, const char* variable);
int spss_filter_select(SPSS_Dataset* ds, const char* expression);

SPSS_Output* spss_frequencies(const SPSS_Dataset* ds, const char* const* vars, size_t nvars);
SPSS_Output* spss_descriptives(const SPSS_Dataset* ds, const char* const* vars, size_t nvars);
SPSS_Output* spss_crosstabs(const SPSS_Dataset* ds, const char* row, const char* col);
SPSS_Output* spss_ttest_onesample(const SPSS_Dataset* ds, const char* var, double testValue);
SPSS_Output* spss_correlation(const SPSS_Dataset* ds, const char* const* vars, size_t nvars);
SPSS_Output* spss_regression(const SPSS_Dataset* ds, const char* dep, const char* const* indep, size_t n);
SPSS_Output* spss_oneway(const SPSS_Dataset* ds, const char* dep, const char* factor);

SPSS_Output* spss_glm(const SPSS_Dataset* ds, const char* dep, const char* const* factors, size_t nfactors,
                      const char* const* covariates, size_t ncov);
SPSS_Output* spss_factor(const SPSS_Dataset* ds, const char* const* vars, size_t nvars, int nfactors);
SPSS_Output* spss_cluster(const SPSS_Dataset* ds, const char* const* vars, size_t nvars, int k);
SPSS_Output* spss_logistic(const SPSS_Dataset* ds, const char* dep, const char* const* indep, size_t n);
SPSS_Output* spss_km(const SPSS_Dataset* ds, const char* time, const char* status);
SPSS_Output* spss_impute(const SPSS_Dataset* ds, const char* const* vars, size_t nvars, int m);
SPSS_Output* spss_bootstrap(const SPSS_Dataset* ds, const char* const* vars, size_t nvars, int samples);
SPSS_Output* spss_forecast(const SPSS_Dataset* ds, const char* var, int periods, int window);
SPSS_Output* spss_ctables(const SPSS_Dataset* ds, const char* row, const char* col, const char* summary);
SPSS_Output* spss_tree(const SPSS_Dataset* ds, const char* dep, const char* predictor);
SPSS_Output* spss_exact(const SPSS_Dataset* ds, const char* row, const char* col);
SPSS_Output* spss_neural(const SPSS_Dataset* ds, const char* dep, const char* const* indep, size_t n);

void spss_output_destroy(SPSS_Output* out);
size_t spss_output_item_count(const SPSS_Output* out);
const char* spss_output_html(const SPSS_Output* out);
const char* spss_last_error(void);

#ifdef __cplusplus
}
#endif
