/********************************************************************
*
*	ServerGame: Nav2 Regression Scenarios
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_bench.h"
#include "svgame/nav2/nav2_serialize.h"


/**
*	@brief	Build the nav2 serialization policy used by static-nav regression scenarios.
*	@return	Serialization policy configured for the static-nav cache transport.
*	@note	Static nav cache regression targets standalone filesystem transport so the policy keeps the transport explicit.
**/
nav2_serialization_policy_t SVG_Nav2_Regression_DefaultStaticNavPolicy( void );

/**
*	@brief	Run the nav2 static-nav serialization round-trip regression scenario via the benchmark harness.
*	@param	policy		Serialization policy describing the payload being validated.
*	@param	outSummary	[out] Optional compact benchmark-facing summary reported by the helper.
*	@param	outDetailedResult	[out] Optional detailed serializer round-trip result for regression diagnostics.
*	@param	scenarioLabel	Optional scenario label override used by the benchmark diagnostics.
*	@return	True if the regression succeeded and the decoded payload matched the source span-grid/adjacency data.
**/
const bool SVG_Nav2_Regression_RunStaticNavRoundTrip( const nav2_serialization_policy_t &policy,
    nav2_bench_roundtrip_summary_t *outSummary = nullptr,
    nav2_serialized_roundtrip_result_t *outDetailedResult = nullptr,
    const char *scenarioLabel = nullptr );

/**
*	@brief	Evaluate one Task 12.3 optimization scenario from benchmark aggregates.
*	@param	scenario	Scenario identifier to evaluate.
*	@param	outResult	[out] Scenario validation output.
*	@return	True when evaluation succeeded.
*	@note	This wrapper keeps Wave 11 regression orchestration localized to regression helpers.
**/
const bool SVG_Nav2_Regression_EvaluateTask123Scenario( const nav2_bench_scenario_t scenario,
    nav2_bench_task123_scenario_result_t *outResult );

/**
*	@brief	Build the full Task 12.3 optimization validation report from benchmark aggregates.
*	@param	outReport	[out] Aggregate Task 12.3 report output.
*	@return	True when report generation succeeded.
**/
const bool SVG_Nav2_Regression_BuildTask123Report( nav2_bench_task123_report_t *outReport );

/**
*	@brief	Emit a compact Task 12.3 regression report to developer console output.
*	@param	report	Task 12.3 report payload to print.
*	@param	detailLimit	Maximum number of per-scenario rows to print.
**/
void SVG_Nav2_Regression_DebugPrintTask123Report( const nav2_bench_task123_report_t &report, const int32_t detailLimit = 12 );
