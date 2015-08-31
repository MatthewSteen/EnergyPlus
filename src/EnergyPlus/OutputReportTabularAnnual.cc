// C++ Headers
#include <vector>
#include <string>
#include <list>
#include <ostream>

// ObjexxFCL Headers
#include <ObjexxFCL/Array1D.hh>
#include <ObjexxFCL/Array1S.hh>
#include <ObjexxFCL/Array2D.hh>
#include <ObjexxFCL/Array2S.hh>
#include <ObjexxFCL/Array3D.hh>
#include <ObjexxFCL/Optional.hh>

// EnergyPlus Headers
#include <OutputReportTabularAnnual.hh>
#include <OutputReportTabular.hh>
#include <InputProcessor.hh>
#include <DataGlobals.hh>
#include <OutputReportData.hh>
#include <DataHVACGlobals.hh>
#include <OutputProcessor.hh>
#include <DataEnvironment.hh>
#include <General.hh>
#include <SQLiteProcedures.hh>


namespace EnergyPlus {

	namespace OutputReportTabularAnnual {

		std::vector<AnnualTable> annualTables;

		void
		GetInputTabularAnnual()
		{
			// Jason Glazer, August 2015
			// The function assigns the input information for
			// REPORT:TABLE:ANNUAL also known as row per object
			// reports that are defined by the user. The input
			// information is assigned to a data structure that
			// is used for both user defined monthly reports and
			// predefined monthly reports.

			static std::string const currentModuleObject( "Output:Table:Annual" );

			int curTable; // index of the current table being processed in MonthlyInput
			int curAggType; // kind of aggregation identified (see AggType parameters)
			std::string curAggString; // Current aggregation sting
			int jAlpha;
			int numParams; // Number of elements combined
			int numAlphas; // Number of elements in the alpha array
			int numNums; // Number of elements in the numeric array
			Array1D_string alphArray; // character string data
			Array1D< Real64 > numArray; // numeric data
			int IOStat; // IO Status when calling get input subroutine
			static bool ErrorsFound( false );
			int objCount( 0 );
			int indexNums( 0 );
			std::string curVarMtr( "" );
			std::string curAggTyp( "" );
			int curNumDgts( 2 );
			AnnualFieldSet::AggregationKind curAgg( AnnualFieldSet::AggregationKind::sumOrAvg );

			objCount = InputProcessor::GetNumObjectsFound( currentModuleObject );
			if ( objCount > 0 ) {
				// if not a run period using weather do not create reports
				if ( !DataGlobals::DoWeathSim ) {
					ShowWarningError( currentModuleObject + " requested with SimulationControl Run Simulation for Weather File Run Periods set to No so " + currentModuleObject + " will not be generated" );
					return;
				}
			}
			InputProcessor::GetObjectDefMaxArgs( currentModuleObject, numParams, numAlphas, numNums );
			alphArray.allocate( numAlphas );
			numArray.dimension( numNums, 0.0 );
			for ( int tabNum = 1 ; tabNum <= objCount; ++tabNum ) {
				InputProcessor::GetObjectItem( currentModuleObject, tabNum, alphArray, numAlphas, numArray, numNums, IOStat );
				if ( numAlphas >= 6 ) {
					annualTables.push_back( AnnualTable(alphArray( 1 ), alphArray( 2 ), alphArray( 3 ), alphArray( 4 ) ));
					// the remaining fields are repeating in groups of three and need to be added to the data structure
					for ( jAlpha = 5; jAlpha <= numAlphas; jAlpha += 2 ) {
						curVarMtr = alphArray( jAlpha );
						if ( jAlpha <= numAlphas ){
							std::string aggregationString = alphArray( jAlpha + 1 );
							curAgg = stringToAggKind( aggregationString );
						}
						else {
							curAgg = AnnualFieldSet::AggregationKind::sumOrAvg; // if missing aggregation type use SumOrAverage
						}
						indexNums = 1 + ( jAlpha - 4 ) / 2; // compute the corresponding field index in the numArray
						if ( indexNums <= numNums ) {
							curNumDgts = numArray( indexNums );
						}
						else {
							curNumDgts = 2;
						}
						annualTables.back().addFieldSet( curVarMtr, curAgg, curNumDgts );
					}
					annualTables.back().setupGathering();
				}
				else	{
					ShowSevereError( currentModuleObject + ": Must enter at least the first six fields." );
				}
			}
		}

		void
		AnnualTable::addFieldSet( std::string varName, AnnualFieldSet::AggregationKind aggKind, int dgts)
		// Jason Glazer, August 2015
		// This method is used along with the constructor to convert the GetInput for REPORT:TABLE:ANNUAL
		// into the class data.
		{
			m_annualFields.push_back(AnnualFieldSet(varName,aggKind, dgts)  );
			m_annualFields.back().m_colHead = InputProcessor::deAllCaps(varName); // use the variable name for the column heading
		};

		void
		AnnualTable::addFieldSet( std::string varName, std::string colName, AnnualFieldSet::AggregationKind aggKind, int dgts )
		// Jason Glazer, August 2015
		// This overloaded method allows for a specific column name to be different than the output variable or meter name
		{
			m_annualFields.push_back( AnnualFieldSet( varName, aggKind, dgts ) );
			m_annualFields.back().m_colHead = colName; // use the user supplied column heading instead of just the variable name
		};


		void
		AnnualTable::setupGathering()
		// Jason Glazer, August 2015
		// This method is used after GetInput for REPORT:TABLE:ANNUAL to set up how output variables, meters, 
		// input fields, and ems variables are gathered. 
		{
			int keyCount = 0;
			int typeVar = 0;
			int avgSumVar = 0;
			int stepTypeVar = 0;
			std::string unitsVar = "";
			Array1D_string namesOfKeys; // keyNames 
			Array1D_int indexesForKeyVar; // keyVarIndexes 
			std::list<std::string> allKeys;

			std::vector<AnnualFieldSet>::iterator fldStIt;
			for ( fldStIt = m_annualFields.begin(); fldStIt != m_annualFields.end(); fldStIt++ )
			{
				keyCount = fldStIt->getVariableKeyCountandTypeFromFldSt( typeVar, avgSumVar, stepTypeVar, unitsVar );
				fldStIt->getVariableKeysFromFldSt( typeVar, keyCount, fldStIt->m_namesOfKeys, fldStIt->m_indexesForKeyVar );
				for ( std::string nm : fldStIt->m_namesOfKeys ){
					allKeys.push_back( nm ); // create list of all items
				}
				fldStIt->m_typeOfVar = typeVar;
				fldStIt->m_varAvgSum = avgSumVar;
				fldStIt->m_varStepType = stepTypeVar;
				fldStIt->m_varUnits = unitsVar;
				fldStIt->m_keyCount = keyCount;
			}
			allKeys.sort();
			allKeys.unique(); // will now just have a list of the unique keys that is sorted
			std::copy( allKeys.begin(), allKeys.end(), back_inserter( m_objectNames) );  // copy list to the object names
			// size all columns list of cells to be the size of the 
			for ( fldStIt = m_annualFields.begin(); fldStIt != m_annualFields.end(); fldStIt++ )
			{
				fldStIt->m_cell.resize( m_objectNames.size() );
			}
			// for each column (field set) set the rows cell to the output variable index (for variables) 
			int foundKeyIndex;
			int tableRowIndex = 0;
			for ( std::vector<std::string>::iterator objNmIt = m_objectNames.begin(); objNmIt != m_objectNames.end(); objNmIt++ ){
				for ( fldStIt = m_annualFields.begin(); fldStIt != m_annualFields.end(); fldStIt++ ){
					foundKeyIndex = -1;
					for ( std::string::size_type i = 0; i < fldStIt->m_namesOfKeys.size(); i++ ){
						if ( fldStIt->m_namesOfKeys[i] == *objNmIt ){
							foundKeyIndex = i;
							break;
						}
					}
					if ( foundKeyIndex > -1 ){
						fldStIt->m_cell[tableRowIndex].indexesForKeyVar = fldStIt->m_indexesForKeyVar[foundKeyIndex];
					}else{
						fldStIt->m_cell[tableRowIndex].indexesForKeyVar = -1; // flag value that cell is not gathered
					}
					fldStIt->m_cell[tableRowIndex].result = 0.0;
					fldStIt->m_cell[tableRowIndex].duration = 0.0;
					fldStIt->m_cell[tableRowIndex].timeStamp = 0;
				}
				tableRowIndex++;
			}
		}


		void
		GatherAnnualResultsForTimeStep( int kindOfTimeStep )
		{
			// Jason Glazer, August 2015
			// This function is not part of the class but acts as an interface between procedural code and the class by
			// gathering data for each of the AnnualTable objects
			std::vector<AnnualTable>::iterator annualTableIt;
			for ( annualTableIt = annualTables.begin(); annualTableIt != annualTables.end(); annualTableIt++ ){
				annualTableIt->gatherForTimestep( kindOfTimeStep );
			}
		}

		void
		AnnualTable::gatherForTimestep( int kindOfTimeStep )
		{
			// Jason Glazer, August 2015
			// For each cell of the table, gather the value as indicated by the type of aggregation
			int const isAverage( 1 );
			int const isSum( 2 );
			int timestepTimeStamp;
			Real64 elapsedTime = AnnualTable::getElapsedTime( kindOfTimeStep );
			Real64 secondsInTimeStep = AnnualTable::getSecondsInTimeStep( kindOfTimeStep );
			bool activeMinMax = false; 
			bool activeHoursShown = false; 
			std::vector<AnnualFieldSet>::iterator fldStIt;
			std::vector<AnnualFieldSet>::iterator fldStRemainIt;
			for ( unsigned int row = 0; row != m_objectNames.size(); row++ ) { //loop through by row.
				for ( fldStIt = m_annualFields.begin(); fldStIt != m_annualFields.end(); fldStIt++ ){
					int curTypeOfVar = fldStIt->m_typeOfVar;
					int curStepType = fldStIt->m_varStepType;
					if ( curStepType == kindOfTimeStep )  // this is a much simpler conditional than the code in monthly gathering
					{
						int curVarNum = fldStIt->m_cell[row].indexesForKeyVar;
						if ( curVarNum > 0 ) {
							Real64 curValue = GetInternalVariableValue( curTypeOfVar, curVarNum );
							// Get the value from the result array
							Real64 oldResultValue = fldStIt->m_cell[row].result;
							int oldTimeStamp = fldStIt->m_cell[row].timeStamp;
							Real64 oldDuration = fldStIt->m_cell[row].duration;
							// Zero the revised values (as default if not set later)
							Real64 newResultValue = 0.0;
							int newTimeStamp = 0;
							Real64 newDuration = 0.0;
							bool activeNewValue = false;
							// the current timestamp
							int minuteCalculated = General::DetermineMinuteForReporting( kindOfTimeStep );
							General::EncodeMonDayHrMin( timestepTimeStamp, DataEnvironment::Month, DataEnvironment::DayOfMonth, DataGlobals::HourOfDay, minuteCalculated );
							// perform the selected aggregation type
							// the following types of aggregations are not gathered at this point: 
							// noAggregation, valueWhenMaxMin, sumOrAverageHoursShown, 	maximumDuringHoursShown, minimumDuringHoursShown:
							switch ( fldStIt->m_aggregate ){
							case AnnualFieldSet::AggregationKind::sumOrAvg:
								if ( fldStIt->m_varAvgSum == isSum ) { // if it is a summed variable
									newResultValue = oldResultValue + curValue;
								}
								else {
									newResultValue = oldResultValue + curValue * elapsedTime; //for averaging - weight by elapsed time
								}
								newDuration = oldDuration + elapsedTime;
								activeNewValue = true;
								break;
							case AnnualFieldSet::AggregationKind::maximum:
								// per MJW when a summed variable is used divide it by the length of the time step
								if ( fldStIt->m_varAvgSum == isSum ) { // if it is a summed variable
									curValue /= secondsInTimeStep;
								}
								if ( curValue > oldResultValue ) {
									newResultValue = curValue;
									newTimeStamp = timestepTimeStamp;
									activeMinMax = true;
									activeNewValue = true;
								}
								else {
									activeMinMax = false; //reset this
								}
								break;
							case AnnualFieldSet::AggregationKind::minimum:
								// per MJW when a summed variable is used divide it by the length of the time step
								if ( fldStIt->m_varAvgSum == isSum ) { // if it is a summed variable
									curValue /= secondsInTimeStep;
								}
								if ( curValue < oldResultValue ) {
									newResultValue = curValue;
									newTimeStamp = timestepTimeStamp;
									activeMinMax = true;
									activeNewValue = true;
								}
								else {
									activeMinMax = false; //reset this
								}
								break;
							case AnnualFieldSet::AggregationKind::hoursNonZero:
								if ( curValue != 0 ) {
									newResultValue = oldResultValue + elapsedTime;
									activeHoursShown = true;
									activeNewValue = true;
								}
								else {
									activeHoursShown = false;
								}
								break;
							case AnnualFieldSet::AggregationKind::hoursZero:
								if ( curValue == 0 ) {
									newResultValue = oldResultValue + elapsedTime;
									activeHoursShown = true;
									activeNewValue = true;
								}
								else {
									activeHoursShown = false;
								}
								break;
							case AnnualFieldSet::AggregationKind::hoursPositive:
								if ( curValue > 0 ) {
									newResultValue = oldResultValue + elapsedTime;
									activeHoursShown = true;
									activeNewValue = true;
								}
								else {
									activeHoursShown = false;
								}
								break;
							case AnnualFieldSet::AggregationKind::hoursNonPositive:
								if ( curValue <= 0 ) {
									newResultValue = oldResultValue + elapsedTime;
									activeHoursShown = true;
									activeNewValue = true;
								}
								else {
									activeHoursShown = false;
								}
								break;
							case AnnualFieldSet::AggregationKind::hoursNegative:
								if ( curValue < 0 ) {
									newResultValue = oldResultValue + elapsedTime;
									activeHoursShown = true;
									activeNewValue = true;
								}
								else {
									activeHoursShown = false;
								}
								break;
							case AnnualFieldSet::AggregationKind::hoursNonNegative:
								if ( curValue >= 0 ) {
									newResultValue = oldResultValue + elapsedTime;
									activeHoursShown = true;
									activeNewValue = true;
								}
								else {
									activeHoursShown = false;
								}
								break;
							case AnnualFieldSet::AggregationKind::hoursInTenPercentBins:
							case AnnualFieldSet::AggregationKind::hoursInTenBinsMinToMax:
							case AnnualFieldSet::AggregationKind::hoursInTenBinsZeroToMax:
							case AnnualFieldSet::AggregationKind::hoursInTenBinsMinToZero:
							case AnnualFieldSet::AggregationKind::hoursInTenBinsPlusMinusTwoStdDev:
							case AnnualFieldSet::AggregationKind::hoursInTenBinsPlusMinusThreeStdDev:
								//  for all of the binning options add the value to the deferred
								if ( fldStIt->m_varAvgSum == isSum ) { // if it is a summed variable
									fldStIt->m_cell[row].deferedResults.push_back( curValue );
								}
								else {
									fldStIt->m_cell[row].deferedResults.push_back( curValue * elapsedTime ); //for averaging - weight by elapsed time
								}
								newDuration = oldDuration + elapsedTime;
								break;
							case AnnualFieldSet::AggregationKind::noAggregation:
							case AnnualFieldSet::AggregationKind::valueWhenMaxMin:
							case AnnualFieldSet::AggregationKind::sumOrAverageHoursShown:
							case AnnualFieldSet::AggregationKind::maximumDuringHoursShown:
							case AnnualFieldSet::AggregationKind::minimumDuringHoursShown:
								// do nothing
								break;
							} // end switch fldStIt->m_aggregate

							// if the new value has been set then set the monthly values to the
							// new columns. This skips the aggregation types that don't even get
							// triggered now such as valueWhenMinMax and all the agg*HoursShown
							if ( activeNewValue ) {
								fldStIt->m_cell[row].result = newResultValue;
								fldStIt->m_cell[row].timeStamp = newTimeStamp;
								fldStIt->m_cell[row].duration = newDuration;
							}
							// if a minimum or maximum value was set this timeStep then
							// scan the remaining columns of the table looking for values
							// that are aggregation type "ValueWhenMaxMin" and set their values
							// if another minimum or maximum column is found then end
							// the scan (it will be taken care of when that column is done)
							if ( activeMinMax ) {
								for ( fldStRemainIt = fldStIt + 1; fldStRemainIt != m_annualFields.end(); fldStRemainIt++ ) {
									if ( fldStRemainIt->m_aggregate == AnnualFieldSet::AggregationKind::maximum || fldStRemainIt->m_aggregate == AnnualFieldSet::AggregationKind::minimum ){
										// end scanning since these might reset
										break; // for fldStRemainIt
									}
									else if ( fldStRemainIt->m_aggregate == AnnualFieldSet::AggregationKind::valueWhenMaxMin ) {
										// this case is when the value should be set
										int scanTypeOfVar = fldStRemainIt->m_typeOfVar;
										int scanStepType = fldStRemainIt->m_varStepType;
										int scanVarNum = fldStRemainIt->m_cell[row].indexesForKeyVar;
										if ( scanVarNum > 0 ){
											Real64 scanValue = GetInternalVariableValue( scanTypeOfVar, scanVarNum );
											// When a summed variable is used divide it by the length of the time step
											if ( fldStRemainIt->m_varAvgSum == isSum ) { // if it is a summed variable
												scanValue /= secondsInTimeStep;
											}
											fldStRemainIt->m_cell[row].result = scanValue;
										}
									} else {
										// do nothing
									}
								}
							}
							// If the hours variable is active then scan through the rest of the variables
							// and accumulate
							if ( activeHoursShown ) {
								for ( fldStRemainIt = fldStIt + 1; fldStRemainIt != m_annualFields.end(); fldStRemainIt++ ) {
									int scanTypeOfVar = fldStRemainIt->m_typeOfVar;
									int scanStepType = fldStRemainIt->m_varStepType;
									int scanVarNum = fldStRemainIt->m_cell[row].indexesForKeyVar;
									Real64 oldScanValue = fldStRemainIt->m_cell[row].result;
									if ( scanVarNum > 0 ){
										Real64 scanValue = GetInternalVariableValue( scanTypeOfVar, scanVarNum );
										if ( fldStRemainIt->m_aggregate == AnnualFieldSet::AggregationKind::hoursZero || fldStRemainIt->m_aggregate == AnnualFieldSet::AggregationKind::hoursNonZero ||
											fldStRemainIt->m_aggregate == AnnualFieldSet::AggregationKind::hoursPositive || fldStRemainIt->m_aggregate == AnnualFieldSet::AggregationKind::hoursNonPositive ||
											fldStRemainIt->m_aggregate == AnnualFieldSet::AggregationKind::hoursNegative || fldStRemainIt->m_aggregate == AnnualFieldSet::AggregationKind::hoursNonNegative ){
											// end scanning since these might reset
											break; // for fldStRemainIt
										}
										else if ( fldStRemainIt->m_aggregate == AnnualFieldSet::AggregationKind::sumOrAverageHoursShown ){
											if ( fldStIt->m_varAvgSum == isSum ) { // if it is a summed variable
												fldStRemainIt->m_cell[row].result = oldScanValue + scanValue;
											}
											else {
												fldStRemainIt->m_cell[row].result = oldScanValue + scanValue * elapsedTime; //for averaging - weight by elapsed time
											}
											fldStRemainIt->m_cell[row].duration += elapsedTime;
										}
										else if ( fldStRemainIt->m_aggregate == AnnualFieldSet::AggregationKind::minimumDuringHoursShown ){
											if ( fldStRemainIt->m_varAvgSum == isSum ) { // if it is a summed variable
												scanValue /= secondsInTimeStep;
											}
											if ( scanValue > oldScanValue ) {
												fldStRemainIt->m_cell[row].result = scanValue;
												fldStRemainIt->m_cell[row].timeStamp = timestepTimeStamp;
											}
										}
										else if ( fldStRemainIt->m_aggregate == AnnualFieldSet::AggregationKind::maximumDuringHoursShown ){
											if ( fldStRemainIt->m_varAvgSum == isSum ) { // if it is a summed variable
												scanValue /= secondsInTimeStep;
											}
											if ( scanValue < oldScanValue ) {
												fldStRemainIt->m_cell[row].result = scanValue;
												fldStRemainIt->m_cell[row].timeStamp = timestepTimeStamp;
											}
										}
										else {
											// do nothing
										}
									}
									activeHoursShown = false; //fixed CR8317
								}
							}
						}
					}
				}
			}
		}

		Real64
		AnnualTable::getElapsedTime( int kindOfTimeStep )
		{
			Real64 elapsedTime;
			if ( kindOfTimeStep == DataGlobals::HVACTSReporting ) {
				elapsedTime = DataHVACGlobals::TimeStepSys;
			} else {
				elapsedTime = DataGlobals::TimeStepZone;
			}
			return elapsedTime;
		}
		
		Real64
		AnnualTable::getSecondsInTimeStep( int kindOfTimeStep )
		{
			Real64 secondsInTimeStep;
			if ( kindOfTimeStep == DataGlobals::HVACTSReporting ) {
				secondsInTimeStep = DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
			}
			else {
				secondsInTimeStep = DataGlobals::TimeStepZoneSec;
			}
			return secondsInTimeStep;
		}


		void
		GatherAnnualOneTimeEntries()
		{

		}


		void
		AnnualTable::gatherOneTimeEntries()
		{

		}

		void
		WriteAnnualTables()
		{
			// Jason Glazer, August 2015
			// This function is not part of the class but acts as an interface between procedural code and the class by
			// invoking the writeTable member function for each of the AnnualTable objects
			std::vector<AnnualTable>::iterator annualTableIt;
			for ( annualTableIt = annualTables.begin(); annualTableIt != annualTables.end(); annualTableIt++ ){
				annualTableIt->writeTable( OutputReportTabular::unitsStyle );
			}
		}

		void
		AnnualTable::writeTable(int unitsStyle)
		{
			Array1D_string columnHead;
			Array1D_int columnWidth;
			Array1D_string rowHead;
			Array2D_string tableBody;
			int const isAverage( 1 );
			int const isSum( 2 );
			Real64 veryLarge = 1.0E280;
			Real64 verySmall = -1.0E280;
			std::vector<std::string> aggString;
			std::string energyUnitsString;
			std::string varNameWithUnits;
			int indexUnitConv;
			Real64 curVal;
			std::string curUnits;
			Real64 curConversionFactor;
			Real64 curConversionOffset;
			Real64 minVal;
			Real64 maxVal;
			Real64 sumVal;
			Real64 sumDuration;

			static Real64 const storedMaxVal( std::numeric_limits< Real64 >::max() );
			static Real64 const storedMinVal( std::numeric_limits< Real64 >::lowest() );

			aggString = setupAggString();
			Real64 energyUnitsConversionFactor = AnnualTable::setEnergyUnitStringAndFactor( unitsStyle, energyUnitsString );

			// first loop through and count how many 'columns' are defined
			// since max and min actually define two columns (the value
			// and the timestamp).
			int columnCount = 0;
			std::vector<AnnualFieldSet>::iterator fldStIt;
			for ( fldStIt = m_annualFields.begin(); fldStIt != m_annualFields.end(); fldStIt++ ){
				columnCount += columnCountForAggregation( fldStIt->m_aggregate );
			} 
			columnHead.allocate( columnCount );
			columnWidth.dimension( columnCount); 
			columnWidth = 14; //array assignment - same for all columns
			int rowCount = m_objectNames.size() + 4; // add blank, sum/avg, min, max rows.
			int rowSumAvg = m_objectNames.size() + 2; 
			int rowMin = m_objectNames.size() + 3;
			int rowMax = m_objectNames.size() + 4;

			rowHead.allocate( rowCount );
			for ( int row = 0; row != m_objectNames.size(); row++ ) { //loop through by row.
				rowHead(row + 1) = m_objectNames[row];
			}
			rowHead( rowSumAvg ) = "Annual Sum or Average";
			rowHead( rowMin ) = "Minimum of Rows";
			rowHead( rowMax ) = "Maximum of Rows";

			tableBody.allocate( columnCount, rowCount );
			tableBody = ""; //set entire table to blank as default
			int columnRecount = 0;
			for ( fldStIt = m_annualFields.begin(); fldStIt != m_annualFields.end(); fldStIt++ ){
				std::string curAggString = aggString[ (int) fldStIt->m_aggregate ];
				if ( curAggString.size() > 0 ) {
					curAggString = " {" + trim( curAggString ) + '}';
				}
				//do the unit conversions
				if ( unitsStyle == OutputReportTabular::unitsStyleInchPound ) {
					varNameWithUnits =  fldStIt->m_variMeter + '[' + fldStIt->m_varUnits + ']';
					OutputReportTabular::LookupSItoIP( varNameWithUnits, indexUnitConv, curUnits );
					OutputReportTabular::GetUnitConversion( indexUnitConv, curConversionFactor, curConversionOffset, curUnits );
				}
				else { //just do the Joule conversion
					//if units is in Joules, convert if specified
					if ( fldStIt->m_varUnits == "J" ) {
						curUnits = energyUnitsString;
						curConversionFactor = energyUnitsConversionFactor;
						curConversionOffset = 0.0;
					}
					else { //if not joules don't perform conversion
						curUnits = fldStIt->m_varUnits;
						curConversionFactor = 1.0;
						curConversionOffset = 0.0;
					}
				}
				int curAgg = fldStIt->m_aggregate;
				columnRecount += columnCountForAggregation( fldStIt->m_aggregate );
				if ( ( curAgg == AnnualFieldSet::AggregationKind::sumOrAvg ) || ( curAgg == AnnualFieldSet::AggregationKind::sumOrAverageHoursShown ) ) {
					// put in the name of the variable for the column
					columnHead( columnRecount ) = fldStIt->m_colHead + curAggString + " [" + curUnits + ']';
					sumVal = 0.0;
					sumDuration = 0.0;
					minVal = storedMaxVal;
					maxVal = storedMinVal;

					for ( int row = 0; row != m_objectNames.size(); row++ ) { //loop through by row.
						if ( fldStIt->m_cell[row].indexesForKeyVar >= 0 ){
							if ( fldStIt->m_varAvgSum == isAverage ) { // if it is a average variable divide by duration
								if ( fldStIt->m_cell[row].duration != 0.0 ) {
									curVal = ( ( fldStIt->m_cell[row].result / fldStIt->m_cell[row].duration ) * curConversionFactor ) + curConversionOffset;
								}
								else {
									curVal = 0.0;
								}
								sumVal += ( fldStIt->m_cell[row].result * curConversionFactor ) + curConversionOffset;
								sumDuration += fldStIt->m_cell[row].duration;
							}
							else {
								curVal = ( fldStIt->m_cell[row].result * curConversionFactor ) + curConversionOffset;
								sumVal += curVal;
							}
							tableBody( columnRecount, row + 1 ) = OutputReportTabular::RealToStr( curVal, fldStIt->m_showDigits );
							if ( curVal > maxVal ) maxVal = curVal;
							if ( curVal < minVal ) minVal = curVal;
						}
						else{
							tableBody( columnRecount, row + 1 ) = "-";
						}

					} //row
					// add the summary to bottom
					if ( fldStIt->m_varAvgSum == isAverage ) { // if it is a average variable divide by duration
						if ( sumDuration > 0 ) {
							tableBody( columnRecount, rowSumAvg ) = OutputReportTabular::RealToStr( sumVal / sumDuration, fldStIt->m_showDigits );
						}
						else {
							tableBody( columnRecount, rowSumAvg ) = "";
						}
					}
					else {
						tableBody( columnRecount, rowSumAvg ) = OutputReportTabular::RealToStr( sumVal, fldStIt->m_showDigits );
					}
					if ( minVal != storedMaxVal ) {
						tableBody( columnRecount, rowMax ) = OutputReportTabular::RealToStr( minVal, fldStIt->m_showDigits );
					}
					if ( maxVal != storedMinVal ) {
						tableBody( columnRecount, rowMin ) = OutputReportTabular::RealToStr( maxVal, fldStIt->m_showDigits );
					}
				}
				else if ( ( curAgg == AnnualFieldSet::AggregationKind::hoursZero ) || ( curAgg == AnnualFieldSet::AggregationKind::hoursNonZero ) || 
					( curAgg == AnnualFieldSet::AggregationKind::hoursPositive ) || ( curAgg == AnnualFieldSet::AggregationKind::hoursNonPositive ) || 
					( curAgg == AnnualFieldSet::AggregationKind::hoursNegative ) || ( curAgg == AnnualFieldSet::AggregationKind::hoursNonNegative ) ) {
					// put in the name of the variable for the column
					columnHead( columnRecount ) = fldStIt->m_colHead + curAggString + " [HOURS]";
					sumVal = 0.0;
					minVal = storedMaxVal;
					maxVal = storedMinVal;
					for ( int row = 0; row != m_objectNames.size(); row++ ) { //loop through by row.
						curVal = fldStIt->m_cell[row].result;
						tableBody( columnRecount, row + 1 ) = OutputReportTabular::RealToStr( curVal, fldStIt->m_showDigits );
						sumVal += curVal;
						if ( curVal > maxVal ) maxVal = curVal;
						if ( curVal < minVal ) minVal = curVal;
					} //row
					// add the summary to bottom
					tableBody( columnRecount, rowSumAvg ) = OutputReportTabular::RealToStr( sumVal, fldStIt->m_showDigits );
					if ( minVal != storedMaxVal ) {
						tableBody( columnRecount, rowMax ) = OutputReportTabular::RealToStr( minVal, fldStIt->m_showDigits );
					}
					if ( maxVal != storedMinVal ) {
						tableBody( columnRecount, rowMin ) = OutputReportTabular::RealToStr( maxVal, fldStIt->m_showDigits );
					}
				}
				else if ( curAgg == AnnualFieldSet::AggregationKind::valueWhenMaxMin ) {
					if ( fldStIt->m_varAvgSum == isSum ) {
						curUnits += "/s";
					}
					fixUnitsPerSecond( curUnits, curConversionFactor );
					columnHead( columnRecount ) = fldStIt->m_colHead + curAggString + " [" + curUnits + ']';
					minVal = storedMaxVal;
					maxVal = storedMinVal;
					for ( int row = 0; row != m_objectNames.size(); row++ ) { //loop through by row.
						curVal = fldStIt->m_cell[row].result;
						tableBody( columnRecount, row + 1 ) = OutputReportTabular::RealToStr( curVal, fldStIt->m_showDigits );
						if ( curVal > maxVal ) maxVal = curVal;
						if ( curVal < minVal ) minVal = curVal;
					} //row
					// add the summary to bottom
					if ( minVal != storedMaxVal ) {
						tableBody( columnRecount, rowMin ) = OutputReportTabular::RealToStr( minVal, fldStIt->m_showDigits );
					}
					if ( maxVal != storedMinVal ) {
						tableBody( columnRecount, rowMax ) = OutputReportTabular::RealToStr( maxVal, fldStIt->m_showDigits );
					}
				}
				else if ( ( curAgg == AnnualFieldSet::AggregationKind::maximum ) || ( curAgg == AnnualFieldSet::AggregationKind::minimum ) ||
					( curAgg == AnnualFieldSet::AggregationKind::maximumDuringHoursShown ) || ( curAgg == AnnualFieldSet::AggregationKind::minimumDuringHoursShown ) ) {
					// put in the name of the variable for the column
					if ( fldStIt->m_varAvgSum == isSum ) { // if it is a summed variable
						curUnits += "/s";
					}
					fixUnitsPerSecond( curUnits, curConversionFactor );
					columnHead( columnRecount - 1 ) = fldStIt->m_colHead + curAggString + '[' + curUnits + ']';
					columnHead( columnRecount ) = fldStIt->m_colHead + " {TIMESTAMP} ";
					minVal = storedMaxVal;
					maxVal = storedMinVal;
					for ( int row = 0; row != m_objectNames.size(); row++ ) { //loop through by row.
						curVal = fldStIt->m_cell[row].result;
						//CR7788 the conversion factors were causing an overflow for the InchPound case since the
						//value was very small
						//restructured the following lines to hide showing HUGE and -HUGE values in output table CR8154 Glazer
						if ( ( curVal < veryLarge ) && ( curVal > verySmall ) ) {
							curVal = curVal * curConversionFactor + curConversionOffset;
							if ( curVal > maxVal ) maxVal = curVal;
							if ( curVal < minVal ) minVal = curVal;
							if ( curVal < veryLarge && curVal > verySmall ) {
								tableBody( columnRecount - 1, row + 1 ) = OutputReportTabular::RealToStr( curVal, fldStIt->m_showDigits );
							}
							else {
								tableBody( columnRecount - 1, row + 1 ) = "-";
							}
							tableBody( columnRecount, row + 1 ) = OutputReportTabular::DateToString( fldStIt->m_cell[row].timeStamp );
						}
						else {
							tableBody( columnRecount - 1, row + 1 ) = "-";
							tableBody( columnRecount, row + 1) = "-";
						}
					} //row
					// add the summary to bottom
					// Don't include if the original min and max values are still present
					if ( minVal < veryLarge ) {
						tableBody( columnRecount - 1, rowMin ) = OutputReportTabular::RealToStr( minVal, fldStIt->m_showDigits );
					}
					else {
						tableBody( columnRecount - 1, rowMin ) = "-";
					}
					if ( maxVal > verySmall ) {
						tableBody( columnRecount - 1, rowMax ) = OutputReportTabular::RealToStr( maxVal, fldStIt->m_showDigits );
					}
					else {
						tableBody( columnRecount - 1, rowMax ) = "-";
					}
				}
				else if ( curAgg == AnnualFieldSet::AggregationKind::hoursInTenBinsMinToMax ){
				}
				else if ( curAgg == AnnualFieldSet::AggregationKind::hoursInTenBinsZeroToMax || curAgg == AnnualFieldSet::AggregationKind::hoursInTenBinsMinToZero ){
				}
				else if ( curAgg == AnnualFieldSet::AggregationKind::hoursInTenPercentBins || curAgg == AnnualFieldSet::AggregationKind::hoursInTenBinsPlusMinusTwoStdDev ||
					curAgg == AnnualFieldSet::AggregationKind::hoursInTenBinsPlusMinusThreeStdDev ){
				}
			} //fldStIt
			OutputReportTabular::WriteReportHeaders( m_name, "Entire Facility", isAverage );
			OutputReportTabular::WriteSubtitle( "Custom Annual Report" );
			OutputReportTabular::WriteTable( tableBody, rowHead, columnHead, columnWidth, true ); //transpose annual XML tables.
			if ( sqlite ) {
				sqlite->createSQLiteTabularDataRecords( tableBody, rowHead, columnHead, m_name, "Entire Facility", "Custom Annual Report" );
			}

		}


		std::vector < std::string >
		AnnualTable::setupAggString()
		{
			std::vector<std::string> retStringVec;
			retStringVec.resize( 20 );
			retStringVec[AnnualFieldSet::AggregationKind::sumOrAvg] = "";
			retStringVec[AnnualFieldSet::AggregationKind::maximum] = " MAXIMUM ";
			retStringVec[AnnualFieldSet::AggregationKind::minimum] = " MINIMUM ";
			retStringVec[AnnualFieldSet::AggregationKind::valueWhenMaxMin] = " AT MAX/MIN ";
			retStringVec[AnnualFieldSet::AggregationKind::hoursZero] = " HOURS ZERO ";
			retStringVec[AnnualFieldSet::AggregationKind::hoursNonZero] = " HOURS NON-ZERO ";
			retStringVec[AnnualFieldSet::AggregationKind::hoursPositive] = " HOURS POSITIVE ";
			retStringVec[AnnualFieldSet::AggregationKind::hoursNonPositive] = " HOURS NON-POSITIVE ";
			retStringVec[AnnualFieldSet::AggregationKind::hoursNegative] = " HOURS NEGATIVE ";
			retStringVec[AnnualFieldSet::AggregationKind::hoursNonNegative] = " HOURS NON-NEGATIVE ";
			retStringVec[AnnualFieldSet::AggregationKind::hoursInTenPercentBins] = " HOURS IN TEN PERCENT BINS ";
			retStringVec[AnnualFieldSet::AggregationKind::hoursInTenBinsMinToMax] = " HOURS IN TEN BINS MIN TO MAX ";
			retStringVec[AnnualFieldSet::AggregationKind::hoursInTenBinsZeroToMax] = " HOURS IN TEN BINS ZERO TO MAX ";
			retStringVec[AnnualFieldSet::AggregationKind::hoursInTenBinsMinToZero] = " HOURS IN TEN BINS MIN TO ZERO ";
			retStringVec[AnnualFieldSet::AggregationKind::hoursInTenBinsPlusMinusTwoStdDev] = " HOURS IN TEN BINS PLUS OR MINUS TWO STD DEV ";
			retStringVec[AnnualFieldSet::AggregationKind::hoursInTenBinsPlusMinusThreeStdDev] = " HOURS IN TEN BINS PLUS OR MINUS THREE STD DEV ";
			retStringVec[AnnualFieldSet::AggregationKind::noAggregation] = " NO AGGREGATION ";
			retStringVec[AnnualFieldSet::AggregationKind::sumOrAverageHoursShown] = " FOR HOURS SHOWN ";
			retStringVec[AnnualFieldSet::AggregationKind::maximumDuringHoursShown] = " MAX FOR HOURS SHOWN ";
			retStringVec[AnnualFieldSet::AggregationKind::minimumDuringHoursShown] = " MIN FOR HOURS SHOWN ";
			return retStringVec;
		}

		Real64
		AnnualTable::setEnergyUnitStringAndFactor( int const unitsStyle, std::string & unitString )
		{
			Real64 convFactor;
			// set the unit conversion
			if ( unitsStyle == OutputReportTabular::unitsStyleNone ) {
				unitString = "J";
				convFactor = 1.0;
			}
			else if ( unitsStyle == OutputReportTabular::unitsStyleJtoKWH ) {
				unitString = "kWh";
				convFactor = 1.0 / 3600000.0;
			}
			else if ( unitsStyle == OutputReportTabular::unitsStyleJtoMJ ) {
				unitString = "MJ";
				convFactor = 1.0 / 1000000.0;
			}
			else if ( unitsStyle == OutputReportTabular::unitsStyleJtoGJ ) {
				unitString = "GJ";
				convFactor = 1.0 / 1000000000.0;
			}
			else { // Should never happen but assures compilers of initialization
				unitString = "J";
				convFactor = 1.0;
			}
			return convFactor;
		}

		void
		AnnualTable::fixUnitsPerSecond( std::string & unitString, Real64 & conversionFactor ){
			if ( unitString == "J/s" ) {
				unitString = "W";
			}
			else if ( unitString == "kWh/s" ) {
				unitString = "W";
				conversionFactor *= 3600000.0;
			}
			else if( unitString == "GJ/s" ) {
				unitString = "kW";
				conversionFactor *= 1000000.0;
			}
			else if( unitString == "MJ/s" ) {
				unitString = "kW";
				conversionFactor *= 1000.0;
			}
			else if( unitString == "therm/s" ) {
				unitString = "kBtu/h";
				conversionFactor *= 360000.0;
			}
			else if( unitString == "kBtu/s" ) {
				unitString = "kBtu/h";
				conversionFactor *= 3600.0;
			}
			else if( unitString == "ton-hrs/s" ) {
				unitString = "ton";
				conversionFactor *= 3600.0;
			}
		}


		AnnualFieldSet::AggregationKind
		stringToAggKind( std::string inString )
		// Jason Glazer, August 2015
		// The function converts a string into an enumeration that describes the type of aggregation 
		// used in REPORT:TABLE:ANNUAL.
		{
			AnnualFieldSet::AggregationKind outAggType;

			if ( InputProcessor::SameString( inString, "SumOrAverage" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::sumOrAvg;
			}
			else if ( InputProcessor::SameString( inString, "Maximum" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::maximum;
			}
			else if ( InputProcessor::SameString( inString, "Minimum" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::minimum;
			}
			else if ( InputProcessor::SameString( inString, "ValueWhenMaximumOrMinimum" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::valueWhenMaxMin;
			}
			else if ( InputProcessor::SameString( inString, "HoursZero" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::hoursZero;
			}
			else if ( InputProcessor::SameString( inString, "HoursNonzero" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::hoursNonZero;
			}
			else if ( InputProcessor::SameString( inString, "HoursPositive" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::hoursPositive;
			}
			else if ( InputProcessor::SameString( inString, "HoursNonpositive" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::hoursNonPositive;
			}
			else if ( InputProcessor::SameString( inString, "HoursNegative" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::hoursNegative;
			}
			else if ( InputProcessor::SameString( inString, "HoursNonNegative" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::hoursNonNegative;
			}
			else if ( InputProcessor::SameString( inString, "HoursInTenPercentBins" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::hoursInTenPercentBins;
			}
			else if ( InputProcessor::SameString( inString, "HourInTenBinsMinToMax" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::hoursInTenBinsMinToMax;
			}
			else if ( InputProcessor::SameString( inString, "HourInTenBinsZeroToMax" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::hoursInTenBinsZeroToMax;
			}
			else if ( InputProcessor::SameString( inString, "HourInTenBinsMinToZero" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::hoursInTenBinsMinToZero;
			}
			else if ( InputProcessor::SameString( inString, "HoursInTenBinsPlusMinusTwoStdDev" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::hoursInTenBinsPlusMinusTwoStdDev;
			}
			else if ( InputProcessor::SameString( inString, "HoursInTenBinsPlusMinusThreeStdDev" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::hoursInTenBinsPlusMinusThreeStdDev;
			}
			else if ( InputProcessor::SameString( inString, "NoAggregation" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::noAggregation;
			}
			else if ( InputProcessor::SameString( inString, "SumOrAverageDuringHoursShown" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::sumOrAverageHoursShown;
			}
			else if ( InputProcessor::SameString( inString, "MaximumDuringHoursShown" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::maximumDuringHoursShown;
			}
			else if ( InputProcessor::SameString( inString, "MinimumDuringHoursShown" ) ) {
				outAggType = AnnualFieldSet::AggregationKind::minimumDuringHoursShown;
			}
			else {
				outAggType = AnnualFieldSet::AggregationKind::sumOrAvg;
				ShowWarningError( "Invalid aggregation type=\"" + inString + "\"  Defaulting to SumOrAverage." );
			}
			return outAggType;
		}


		int
		AnnualTable::columnCountForAggregation( AnnualFieldSet::AggregationKind curAgg ){
			int	returnCount = 0;
			if ( curAgg == AnnualFieldSet::AggregationKind::sumOrAvg || curAgg == AnnualFieldSet::AggregationKind::valueWhenMaxMin ||
				curAgg == AnnualFieldSet::AggregationKind::hoursZero || curAgg == AnnualFieldSet::AggregationKind::hoursNonZero ||
				curAgg == AnnualFieldSet::AggregationKind::hoursPositive || curAgg == AnnualFieldSet::AggregationKind::hoursNonPositive ||
				curAgg == AnnualFieldSet::AggregationKind::hoursNegative || curAgg == AnnualFieldSet::AggregationKind::hoursNonNegative ||
				curAgg == AnnualFieldSet::AggregationKind::sumOrAverageHoursShown || curAgg == AnnualFieldSet::AggregationKind::noAggregation ) {
				returnCount = 1;
			}
			else if ( curAgg == AnnualFieldSet::AggregationKind::maximum || curAgg == AnnualFieldSet::AggregationKind::minimum ||
				curAgg == AnnualFieldSet::AggregationKind::maximumDuringHoursShown || curAgg == AnnualFieldSet::AggregationKind::minimumDuringHoursShown ) {
				returnCount = 2;
			}
			else if ( curAgg == AnnualFieldSet::AggregationKind::hoursInTenBinsMinToMax ){
				returnCount = 10;
			}
			else if ( curAgg == AnnualFieldSet::AggregationKind::hoursInTenBinsZeroToMax || curAgg == AnnualFieldSet::AggregationKind::hoursInTenBinsMinToZero ){
				returnCount = 11;
			}
			else if ( curAgg == AnnualFieldSet::AggregationKind::hoursInTenPercentBins || curAgg == AnnualFieldSet::AggregationKind::hoursInTenBinsPlusMinusTwoStdDev ||
				curAgg == AnnualFieldSet::AggregationKind::hoursInTenBinsPlusMinusThreeStdDev ){
				returnCount = 12;
			}
			return returnCount;
		}

		std::string 
		AnnualTable::trim( const std::string& str )
		{
			std::string whitespace = " \t";
			const auto strBegin = str.find_first_not_of( whitespace );
			if ( strBegin == std::string::npos )
				return ""; // no content

			const auto strEnd = str.find_last_not_of( whitespace );
			const auto strRange = strEnd - strBegin + 1;

			return str.substr( strBegin, strRange );
		}


		void
		AddAnnualTableOfContents( std::ostream & nameOfStream){
			// Jason Glazer, August 2015
			// This function is not part of the class but acts as an interface between procedural code and the class by
			// invoking the writeTable member function for each of the AnnualTable objects
			std::vector<AnnualTable>::iterator annualTableIt;
			for ( annualTableIt = annualTables.begin(); annualTableIt != annualTables.end(); annualTableIt++ ){
				annualTableIt->addTableOfContents( nameOfStream );
			}
		}

		void
		AnnualTable::addTableOfContents( std::ostream & nameOfStream ){
			nameOfStream << "<p><b>" << m_name << "</b></p> |\n";
			nameOfStream << "<a href=\"#" << OutputReportTabular::MakeAnchorName( m_name, "Entire Facility" ) << "\">" << "Entire Facility" << "</a>    |   \n";
		}


	} //OutputReportTabularAnnual

} // EnergyPlus



	//     NOTICE

	//     Copyright (c) 1996-2015 The Board of Trustees of the University of Illinois
	//     and The Regents of the University of
	//     Berkeley National Laboratory.  All rights reserved.

	//     Portions of the EnergyPlus software package have been developed and copyrighted
	//     by other individuals, companies and institutions.  These portions have been
	//     incorporated into the EnergyPlus software package under license.   For a complete
	//     list of contributors, see "Notice" located in main.cc.

	//     NOTICE: The U.S. Government is granted for itself and others acting on its
	//     behalf a paid-up, nonexclusive, irrevocable, worldwide license in this data to
	//     reproduce, prepare derivative works, and perform publicly and display publicly.
	//     Beginning five (5) years after permission to assert copyright is granted,
	//     subject to two possible five year renewals, the U.S. Government is granted for
	//     itself and others acting on its behalf a paid-up, non-exclusive, irrevocable
	//     worldwide license in this data to reproduce, prepare derivative works,
	//     distribute copies to the public, perform publicly and display publicly, and to
	//     permit others to do so.

	//     TRADEMARKS: EnergyPlus is a trademark of the US Department of Energy.


