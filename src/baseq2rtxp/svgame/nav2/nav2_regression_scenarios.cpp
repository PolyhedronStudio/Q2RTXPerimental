/********************************************************************
*
*	ServerGame: Nav2 Regression Scenarios - Implementation
*
********************************************************************/
#include "svgame/nav2/nav2_regression_scenarios.h"

#include "svgame/nav2/nav2_format.h"
#include "svgame/nav2/nav2_span_adjacency.h"
#include "svgame/nav2/nav2_span_grid_build.h"


/**
*	@brief	Build the nav2 serialization policy used by static-nav regression scenarios.
*	@return	Serialization policy configured for the static-nav cache transport.
*	@note	Static nav cache regression targets standalone filesystem transport so the policy keeps the transport explicit.
**/
nav2_serialization_policy_t SVG_Nav2_Regression_DefaultStaticNavPolicy( void ) {
	/**
	*	Seed the default policy with explicit transport and category values.
	**/
	// Initialize the policy with deterministic defaults.
	nav2_serialization_policy_t policy = {};
	// Mark the payload as a static asset for cache validation.
	policy.category = nav2_serialized_category_t::StaticAsset;
	// Require the standalone filesystem transport used by cache files.
	policy.transport = nav2_serialized_transport_t::EngineFilesystem;
	// Leave the map identity unset until a caller provides a hash token.
	policy.map_identity = 0;
	return policy;
}

/**
*	@brief	Run the nav2 static-nav serialization round-trip regression scenario via the benchmark harness.
*	@param	policy		Serialization policy describing the payload being validated.
*	@param	outSummary	[out] Optional compact benchmark-facing summary reported by the helper.
*	@param	outDetailedResult	[out] Optional detailed serializer round-trip result for regression diagnostics.
*	@param	scenarioLabel	Optional scenario label override used by the benchmark diagnostics.
*	@return	True if the regression succeeded and the decoded payload matched the source span-grid/adjacency data.
**/
const bool SVG_Nav2_Regression_RunStaticNavRoundTrip( const nav2_serialization_policy_t &policy,
	nav2_bench_roundtrip_summary_t *outSummary,
	nav2_serialized_roundtrip_result_t *outDetailedResult,
	const char *scenarioLabel ) {
	/**
	*	Reset optional outputs up front so failure paths never leak stale data.
	**/
	// Clear the optional benchmark summary output.
	if ( outSummary ) {
		*outSummary = nav2_bench_roundtrip_summary_t{};
	}
	// Clear the optional detailed serializer result output.
	if ( outDetailedResult ) {
		*outDetailedResult = nav2_serialized_roundtrip_result_t{};
	}

	/**
	*	Build the span grid from the current runtime mesh so the regression uses live static nav data.
	**/
	// Build the span grid from the active nav2 runtime mesh.
	nav2_span_grid_t spanGrid = {};
	if ( !SVG_Nav2_BuildSpanGrid( &spanGrid ) ) {
		// Return false when the span grid build could not run.
		return false;
	}

	/**
	*	Build adjacency from the generated span grid so the round-trip includes connectivity data.
	**/
	// Build adjacency based on the newly created span grid.
	nav2_span_adjacency_t adjacency = {};
	if ( !SVG_Nav2_BuildSpanAdjacency( spanGrid, &adjacency ) ) {
		// Return false when adjacency could not be generated.
		return false;
	}

	/**
	*	Run the serializer round-trip through the benchmark helper so timing and mismatch diagnostics are captured.
	**/
	// Invoke the benchmark harness helper to execute the round-trip.
	return SVG_Nav2_Bench_RunStaticNavRoundTrip( policy, spanGrid, adjacency, outSummary, outDetailedResult, scenarioLabel );
}

/**
*	@brief	Evaluate one Task 12.3 optimization scenario from benchmark aggregates.
*	@param	scenario	Scenario identifier to evaluate.
*	@param	outResult	[out] Scenario validation output.
*	@return	True when evaluation succeeded.
*	@note	This wrapper keeps Wave 11 regression orchestration localized to regression helpers.
**/
const bool SVG_Nav2_Regression_EvaluateTask123Scenario( const nav2_bench_scenario_t scenario,
	nav2_bench_task123_scenario_result_t *outResult ) {
	/**
	*	Require output storage and delegate to the benchmark-owned evaluator.
	**/
	if ( !outResult ) {
		return false;
	}
	return SVG_Nav2_Bench_EvaluateTask123Scenario( scenario, outResult );
}

/**
*	@brief	Build the full Task 12.3 optimization validation report from benchmark aggregates.
*	@param	outReport	[out] Aggregate Task 12.3 report output.
*	@return	True when report generation succeeded.
**/
const bool SVG_Nav2_Regression_BuildTask123Report( nav2_bench_task123_report_t *outReport ) {
	/**
	*	Require output storage and delegate to the benchmark-owned report builder.
	**/
	if ( !outReport ) {
		return false;
	}
	return SVG_Nav2_Bench_BuildTask123Report( outReport );
}

/**
*	@brief	Emit a compact Task 12.3 regression report to developer console output.
*	@param	report	Task 12.3 report payload to print.
*	@param	detailLimit	Maximum number of per-scenario rows to print.
**/
void SVG_Nav2_Regression_DebugPrintTask123Report( const nav2_bench_task123_report_t &report, const int32_t detailLimit ) {
	/**
	*	Print aggregate counters first so developers can quickly see global pass/fail status.
	**/
	gi.dprintf( "[Nav2Task12.3] passed=%d scenarios=%u with_records=%u pass=%u fail=%u opt_used=%u opt_helped=%u opt_regressed=%u correctness=%u many_agent=%u serialized_reuse=%u\n",
		report.passed ? 1 : 0,
		report.scenario_count,
		report.scenarios_with_records,
		report.scenario_pass_count,
		report.scenario_fail_count,
		report.optimization_used_count,
		report.optimization_helped_count,
		report.optimization_regressed_count,
		report.correctness_preserved_count,
		report.many_agent_scenario_count,
		report.serialized_reuse_scenario_count );

	/**
	*	Emit bounded per-scenario rows to keep diagnostics readable and avoid log spam.
	**/
	const int32_t clampedLimit = std::max( 0, detailLimit );
	int32_t printed = 0;
	for ( size_t scenarioIndex = 0; scenarioIndex < ( size_t )nav2_bench_scenario_t::Count; scenarioIndex++ ) {
		const nav2_bench_task123_scenario_result_t &scenarioResult = report.scenario_results[ scenarioIndex ];
		if ( scenarioResult.profile == nav2_bench_task123_profile_t::Unknown && !scenarioResult.has_record ) {
			continue;
		}
		if ( clampedLimit > 0 && printed >= clampedLimit ) {
			break;
		}

		const char *scenarioName = SVG_Nav2_Bench_ScenarioName( scenarioResult.scenario );
		const char *profileName = SVG_Nav2_Bench_Task123ProfileName( scenarioResult.profile );
		gi.dprintf( "[Nav2Task12.3] scenario=%s profile=%s has_record=%d passed=%d used=%d helped=%d regressed=%d correct=%d total_ms=%.3f coarse_ms=%.3f fine_ms=%.3f queue_ms=%.3f worker_ms=%.3f stability=%.3f connector=%.3f failure=%.3f signals=0x%08x\n",
			scenarioName ? scenarioName : "Unknown",
			profileName ? profileName : "Unknown",
			scenarioResult.has_record ? 1 : 0,
			scenarioResult.passed ? 1 : 0,
			scenarioResult.metrics.optimization_used ? 1 : 0,
			scenarioResult.metrics.optimization_helped ? 1 : 0,
			scenarioResult.metrics.optimization_regressed ? 1 : 0,
			scenarioResult.metrics.correctness_preserved ? 1 : 0,
			scenarioResult.metrics.avg_total_query_ms,
			scenarioResult.metrics.avg_coarse_search_ms,
			scenarioResult.metrics.avg_fine_refinement_ms,
			scenarioResult.metrics.avg_worker_queue_wait_ms,
			scenarioResult.metrics.avg_worker_execution_ms,
			scenarioResult.metrics.route_stability_score,
			scenarioResult.metrics.connector_selection_score,
			scenarioResult.metrics.failure_pressure_score,
			scenarioResult.metrics.signal_bits );
		printed++;
	}
}
