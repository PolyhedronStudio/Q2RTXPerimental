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
