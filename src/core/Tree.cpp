// Copyright 2007,2008,2009,2010,2011,2012,2013,2014,2015 Loïc Cerf (lcerf@dcc.ufmg.br)

// This file is part of multidupehack.

// multidupehack is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 3 as published by the Free Software Foundation

// multidupehack is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along with multidupehack; if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

#include "Tree.h"

const bool smallerDimension(const Dimension* dimension, const Dimension* otherDimension)
{
  return *dimension < *otherDimension;
}

#ifdef TIME
steady_clock::time_point Tree::overallBeginning;
#endif
#ifdef DETAILED_TIME
steady_clock::time_point Tree::startingPoint;
double Tree::parsingDuration;
double Tree::preProcessingDuration;
#endif
#ifdef MIN_SIZE_ELEMENT_PRUNING_TIME
double Tree::minSizeElementPruningDuration = 0;
#endif
#ifdef NB_OF_LEFT_NODES
unsigned int Tree::nbOfLeftNodes = 0;
#endif
#ifdef NB_OF_CLOSED_N_SETS
unsigned int Tree::nbOfClosedNSets = 0;
#endif

vector<unsigned int> Tree::external2InternalAttributeOrder;
vector<unordered_map<string, unsigned int>> Tree::labels2Ids;
unsigned int Tree::firstSymmetricAttributeId;
unsigned int Tree::lastSymmetricAttributeId;
Trie* Tree::data;
vector<unsigned int> Tree::minSizes;
double Tree::minArea;

bool Tree::isAgglomeration;
ofstream Tree::outputFile;
string Tree::outputDimensionSeparator;
string Tree::patternSizeSeparator;
string Tree::sizeSeparator;
string Tree::sizeAreaSeparator;
bool Tree::isSizePrinted;
bool Tree::isAreaPrinted;

Tree::Tree(const char* dataFileName, const float densityThreshold, const double shiftMultiplier, const vector<double>& epsilonVectorParam, const vector<unsigned int>& cliqueDimensionsParam, const vector<double>& tauVectorParam, const vector<unsigned int>& minSizesParam, const unsigned int minAreaParam, const bool isReductionOnly, const bool isAgglomerationParam, const vector<unsigned int>& unclosedDimensions, const char* inputElementSeparator, const char* inputDimensionSeparator, const char* outputFileName, const char* outputDimensionSeparatorParam, const char* patternSizeSeparatorParam, const char* sizeSeparatorParam, const char* sizeAreaSeparatorParam, const bool isSizePrintedParam, const bool isAreaPrintedParam) : attributes(), mereConstraints(), isEnumeratedElementPotentiallyPreventingClosedness(false)
{
#ifdef TIME
  overallBeginning = steady_clock::now();
#endif
  vector<unsigned int> numDimensionIds;
  const vector<unsigned int>::const_iterator cliqueDimensionEnd = cliqueDimensionsParam.end();
  vector<unsigned int>::const_iterator cliqueDimensionIt = cliqueDimensionsParam.begin();
  unsigned int dimensionId = 0;
  for (const double tau : tauVectorParam)
    {
      if (tau != 0)
	{
	  for (; cliqueDimensionIt != cliqueDimensionEnd && *cliqueDimensionIt < dimensionId; ++cliqueDimensionIt)
	    {
	    }
	  if (cliqueDimensionIt != cliqueDimensionEnd && *cliqueDimensionIt == dimensionId)
	    {
	      throw UsageException(("clique and tau options indicate that attribute " + lexical_cast<string>(*cliqueDimensionIt) + " is both symmetric and almost-contiguous. This is not supported yet. Would you implement that feature?").c_str());
	    }
	  numDimensionIds.push_back(dimensionId);
	}
      ++dimensionId;
    }
#ifdef DETAILED_TIME
  startingPoint = steady_clock::now();
#endif
  NoisyTupleFileReader noisyTupleFileReader(dataFileName, cliqueDimensionsParam, numDimensionIds, inputDimensionSeparator, inputElementSeparator);
  pair<vector<unsigned int>, double> noisyTuple = noisyTupleFileReader.next();
  const unsigned int n = noisyTuple.first.size();
  if (tauVectorParam.size() > n)
    {
      throw UsageException(("tau option should provide at most " + lexical_cast<string>(n) + " coefficients!").c_str());
    }
  if (minSizesParam.size() > n)
    {
      throw UsageException(("sizes option should provide at most " + lexical_cast<string>(n) + " sizes!").c_str());
    }
  if (epsilonVectorParam.size() > n)
    {
      throw UsageException(("epsilon option should provide at most " + lexical_cast<string>(n) + " coefficients!").c_str());
    }
  isAgglomeration = isAgglomerationParam;
  if (!isAgglomeration)
    {
      double minMembership = 1;
      if (epsilonVectorParam.size() == n)
	{
	  minMembership -= *min_element(epsilonVectorParam.begin(), epsilonVectorParam.end());
	}
      if (minMembership > 1. / numeric_limits<unsigned int>::max())
	{
	  noisyTupleFileReader.setMinMembership(minMembership);
	  if (minMembership > noisyTuple.second)
	    {
	      noisyTupleFileReader.startOverFromNextLine();
	      noisyTuple = noisyTupleFileReader.next();
	    }
	}
    }
  // Initialize minSizeVector and find out whether the pre-process is wanted/useful
  bool isToBePreProcessed = true;
  minArea = minAreaParam;
  vector<unsigned int> minSizeVector = minSizesParam;
  if (minArea == 0)
    {
      if (minSizeVector.size() != n)
	{
	  isToBePreProcessed = false;
	  minSizeVector.resize(n);
	}
      else
	{
	  for (const unsigned int minSize : minSizeVector)
	    {
	      if (minSize == 0)
		{
		  isToBePreProcessed = false;
		  break;
		}
	    }
	}
    }
  else
    {
      minSizeVector.resize(n, 1);
    }
  // Parse
  // TODO: one copy of the data if isToBePreProcessed is false (and add an option to disable the pre-process)
  vector<vector<NoisyTuples*>> hyperplanes(n);
  bool isCrisp = true;
  for (; noisyTuple.second != 0; noisyTuple = noisyTupleFileReader.next())
    {
      isCrisp = isCrisp && noisyTuple.second == 1;
      dimensionId = 0;
      vector<unsigned int>::const_iterator cliqueDimensionIt = cliqueDimensionsParam.begin();
      vector<unsigned int> projectedTuple(noisyTuple.first.begin() + 1, noisyTuple.first.end());
      vector<unsigned int>::iterator projectedTupleIt = projectedTuple.begin();
      vector<unsigned int>::const_iterator elementIt = noisyTuple.first.begin();
      for (vector<vector<NoisyTuples*>>::iterator hyperplanesInDimensionIt = hyperplanes.begin(); ; ++hyperplanesInDimensionIt)
	{
	  if (cliqueDimensionIt != cliqueDimensionEnd && *cliqueDimensionIt == dimensionId++)
	    {
	      ++cliqueDimensionIt;
	      while (*elementIt >= hyperplanesInDimensionIt->size())
		{
		  for (const unsigned int cliqueDimension2 : cliqueDimensionsParam)
		    {
		      hyperplanes[cliqueDimension2].push_back(new NoisyTuples());
		    }
		}
	    }
	  else
	    {
	      while (*elementIt >= hyperplanesInDimensionIt->size())
		{
		  hyperplanesInDimensionIt->push_back(new NoisyTuples());
		}
	    }
	  (*hyperplanesInDimensionIt)[*elementIt]->insert(projectedTuple, noisyTuple.second);
	  if (projectedTupleIt == projectedTuple.end())
	    {
	      break;
	    }
	  *projectedTupleIt++ = *elementIt++;
	}
    }
  vector<unsigned int> cardinalities = noisyTupleFileReader.getCardinalities();
  vector<vector<NoisyTuples*>>::iterator hyperplanesInDimensionIt = hyperplanes.begin();
  for (const unsigned int cardinality : cardinalities)
    {
      while (hyperplanesInDimensionIt->size() != cardinality)
	{
	  hyperplanesInDimensionIt->push_back(new NoisyTuples());
	}
      ++hyperplanesInDimensionIt;
    }
#ifdef DETAILED_TIME
  parsingDuration = duration_cast<duration<double>>(steady_clock::now() - startingPoint).count();
  startingPoint = steady_clock::now();
#endif
  // Initialize epsilonVector, minimalNbOfNonSelfLoopTuples and maximalNbOfNonSelfLoopTuples and cardinalities considering the input data order of the attributes
  vector<double> epsilonVector = epsilonVectorParam;
  epsilonVector.resize(n);
  // If the data is crisp, reset the epsilons to values with .5 as the decimal part
  if (isCrisp)
    {
      const vector<double>::iterator epsilonEnd = epsilonVector.end();
      vector<double>::iterator epsilonIt = epsilonVector.begin();
      for (; epsilonIt != epsilonEnd && *epsilonIt >= 1; ++epsilonIt)
	{
	  *epsilonIt = floor(*epsilonIt) + .5;
	}
      if (epsilonIt != epsilonEnd)
	{
	  cerr << "Warning: crisp relation (d-peeler applicable and probably faster)" << endl;
	  epsilonVector = vector<double>(n, .5);
	}
    }
  vector<unsigned int> minimalNbOfNonSelfLoopTuples;
  minimalNbOfNonSelfLoopTuples.reserve(n);
  vector<unsigned int> nbOfNonSelfLoopTuples;
  nbOfNonSelfLoopTuples.reserve(n);
  unsigned int minNbOfSymmetricElements = 0;
  if (cliqueDimensionsParam.empty())
    {
      IndistinctSkyPatterns::setParametersToComputePresentAndPotentialIrrelevancyThresholds(numeric_limits<unsigned int>::max(), 0);
      for (dimensionId = 0; dimensionId != n; ++dimensionId)
	{
	  minimalNbOfNonSelfLoopTuples.push_back(IndistinctSkyPatterns::nbOfNonSelfLoopTuplesInHyperplaneOfPattern(minSizeVector, dimensionId, 0));
	  nbOfNonSelfLoopTuples.push_back(IndistinctSkyPatterns::nbOfNonSelfLoopTuplesInHyperplaneOfPattern(cardinalities, dimensionId, 0));
	}
    }
  else
    {
      // Move the number and the minimal number of symmetric elements at the end (they need to be contiguous when calling IndistinctSkyPatterns::nbOfNonSelfLoopTuplesInHyperplaneOfPattern)
      vector<unsigned int> reorderedCardinalities;
      reorderedCardinalities.reserve(cardinalities.size());
      dimensionId = 0;
      vector<unsigned int>::const_iterator cliqueDimensionIt = cliqueDimensionsParam.begin();
      for (const unsigned int cardinality : cardinalities)
	{
	  if (cliqueDimensionIt != cliqueDimensionEnd && *cliqueDimensionIt == dimensionId++)
	    {
	      ++cliqueDimensionIt;
	    }
	  else
	    {
	      reorderedCardinalities.push_back(cardinality);
	    }
	}
      reorderedCardinalities.insert(reorderedCardinalities.end(), cliqueDimensionsParam.size(), cardinalities[cliqueDimensionsParam.front()]);
      vector<unsigned int> reorderedMinSizes;
      reorderedMinSizes.reserve(minSizeVector.size());
      dimensionId = 0;
      cliqueDimensionIt = cliqueDimensionsParam.begin();
      for (const unsigned int minSize : minSizeVector)
	{
	  if (cliqueDimensionIt != cliqueDimensionEnd && *cliqueDimensionIt == dimensionId++)
	    {
	      ++cliqueDimensionIt;
	      if (minSize > minNbOfSymmetricElements)
		{
		  minNbOfSymmetricElements = minSize;
		}
	    }
	  else
	    {
	      reorderedMinSizes.push_back(minSize);
	    }
	}
      reorderedMinSizes.insert(reorderedMinSizes.end(), cliqueDimensionsParam.size(), minNbOfSymmetricElements);
      for (const unsigned int cliqueDimensionId : cliqueDimensionsParam)
	{
	  minSizeVector[cliqueDimensionId] = minNbOfSymmetricElements;
	}
      IndistinctSkyPatterns::setParametersToComputePresentAndPotentialIrrelevancyThresholds(n - cliqueDimensionsParam.size(), n - 1);
      const unsigned int nbOfNonSelfLoopTuplesInSymmetricHyperplane = IndistinctSkyPatterns::nbOfNonSelfLoopTuplesInHyperplaneOfPattern(reorderedCardinalities, n - 1, reorderedCardinalities.back());
      const unsigned int minimalNbOfNonSelfLoopTuplesInSymmetricHyperplane = IndistinctSkyPatterns::nbOfNonSelfLoopTuplesInHyperplaneOfPattern(reorderedMinSizes, n - 1, minNbOfSymmetricElements);
      unsigned int internalDimensionId = 0;
      cliqueDimensionIt = cliqueDimensionsParam.begin();
      for (dimensionId = 0; dimensionId != n; ++dimensionId)
	{
	  if (cliqueDimensionIt != cliqueDimensionEnd && *cliqueDimensionIt == dimensionId)
	    {
	      ++cliqueDimensionIt;
	      nbOfNonSelfLoopTuples.push_back(nbOfNonSelfLoopTuplesInSymmetricHyperplane);
	      minimalNbOfNonSelfLoopTuples.push_back(minimalNbOfNonSelfLoopTuplesInSymmetricHyperplane);
	    }
	  else
	    {
	      nbOfNonSelfLoopTuples.push_back(IndistinctSkyPatterns::nbOfNonSelfLoopTuplesInHyperplaneOfPattern(reorderedCardinalities, internalDimensionId, reorderedCardinalities.back()));
	      minimalNbOfNonSelfLoopTuples.push_back(IndistinctSkyPatterns::nbOfNonSelfLoopTuplesInHyperplaneOfPattern(reorderedMinSizes, internalDimensionId, minNbOfSymmetricElements));
	      ++internalDimensionId;
	    }
	}
    }
  vector<Dimension*> dimensions;
#ifndef PRE_PROCESS
  isToBePreProcessed = false;
#endif
  if (isToBePreProcessed)
    {
      // Pre-process
      // PERF: if !isAgglomeration, ignore the tuples with noise above min(epsilon); they were inserted so that they are considered when agglomerating but cannot be in a closed n-set
      dimensions = NoisyTuples::preProcess(nbOfNonSelfLoopTuples, minimalNbOfNonSelfLoopTuples, epsilonVector, cliqueDimensionsParam, hyperplanes);
    }
  else
    {
      dimensions.reserve(n);
      dimensionId = 0;
      vector<unsigned int>::const_iterator cliqueDimensionIt = cliqueDimensionsParam.begin();
      for (const unsigned int cardinality : cardinalities)
	{
	  if (cliqueDimensionIt != cliqueDimensionEnd && *cliqueDimensionIt == dimensionId)
	    {
	      ++cliqueDimensionIt;
	      dimensions.push_back(new Dimension(dimensionId++, cardinality, true));
	    }
	  else
	    {
	      dimensions.push_back(new Dimension(dimensionId++, cardinality, false));
	    }
	}
    }
#ifdef DETAILED_TIME
  preProcessingDuration = duration_cast<duration<double>>(steady_clock::now() - startingPoint).count();
#endif
#ifdef OUTPUT
  outputFile.open(outputFileName);
  if (!outputFile)
    {
      throw NoFileException(outputFileName);
    }
#endif
  if (dimensions.front()->getCardinality() == 0)
    {
      // The pre-process erased all tuples
      for (Dimension* dimension : dimensions)
	{
	  delete dimension;
	}
      for (vector<NoisyTuples*>& hyperplanesInDimension : hyperplanes)
	{
	  for (NoisyTuples* hyperplane : hyperplanesInDimension)
	    {
	      delete hyperplane;
	    }
	}
      return;
    }
  if (isReductionOnly)
    {
      // Delete the n - 1 last copies of the reduced data
      for (hyperplanesInDimensionIt = hyperplanes.begin(); ++hyperplanesInDimensionIt != hyperplanes.end(); )
	{
	  for (NoisyTuples* hyperplane : *hyperplanesInDimensionIt)
	    {
	      delete hyperplane;
	    }
	}
      // Output the reduced data
      unsigned int firstDimensionHyperplaneId = 0;
      bool isFirst = true;
      vector<NoisyTuples*>& hyperplanesInFirstDimension = hyperplanes.front();
      for (NoisyTuples* hyperplane : hyperplanesInFirstDimension)
	{
#ifdef OUTPUT
	  if (!hyperplane->empty())
	    {
	      if (isFirst)
		{
		  isFirst = false;
		}
	      else
		{
		  outputFile << endl;
		}
	      noisyTupleFileReader.printTuplesInFirstDimensionHyperplane(outputFile, firstDimensionHyperplaneId, hyperplane->begin(), hyperplane->end(), outputDimensionSeparatorParam);
	    }
	  ++firstDimensionHyperplaneId;
#endif
	  delete hyperplane;
	}
      for (Dimension* dimension : dimensions)
	{
	  delete dimension;
	}
#ifdef OUTPUT
      outputFile << endl;
#endif
      return;
    }
  // Init some static variables
  outputDimensionSeparator = outputDimensionSeparatorParam;
  patternSizeSeparator = patternSizeSeparatorParam;
  sizeSeparator = sizeSeparatorParam;
  sizeAreaSeparator = sizeAreaSeparatorParam;
  isSizePrinted = isSizePrintedParam;
  isAreaPrinted = isAreaPrintedParam;
  // Order the dimensions by increasing cardinality
  sort(dimensions.begin(), dimensions.end(), smallerDimension);
  external2InternalAttributeOrder.resize(n);
  unsigned int attributeId = 0;
  vector<unsigned int> internal2ExternalAttributeOrder;
  internal2ExternalAttributeOrder.reserve(n);
  for (const Dimension* dimension : dimensions)
    {
      dimensionId = dimension->getId();
      internal2ExternalAttributeOrder.push_back(dimensionId);
      external2InternalAttributeOrder[dimensionId] = attributeId++;
    }
#if defined DEBUG || defined ASSERT
  Attribute::setInternal2ExternalAttributeOrder(internal2ExternalAttributeOrder);
#endif
#ifdef DEBUG
  Measure::setInternal2ExternalAttributeOrder(internal2ExternalAttributeOrder);
#endif
  // Define symmetric attributes ids accordingly
  firstSymmetricAttributeId = numeric_limits<unsigned int>::max();
  lastSymmetricAttributeId = 0;
  for (const unsigned int cliqueDimension : cliqueDimensionsParam)
    {
      const unsigned int symmetricAttributeId = external2InternalAttributeOrder[cliqueDimension];
      if (symmetricAttributeId < firstSymmetricAttributeId)
	{
	  firstSymmetricAttributeId = symmetricAttributeId;
	}
      if (symmetricAttributeId > lastSymmetricAttributeId)
	{
	  lastSymmetricAttributeId = symmetricAttributeId;
	}
    }
#ifdef MIN_SIZE_ELEMENT_PRUNING
  // Initialize parameters to compute presentAndPotentialIrrelevancyThresholds given the sky-patterns
  IndistinctSkyPatterns::setParametersToComputePresentAndPotentialIrrelevancyThresholds(firstSymmetricAttributeId, lastSymmetricAttributeId);
#endif
  // Initialize cardinalities and oldIds2NewIds according to the new attribute order, delete the elements in dimensions and the hyperplanes not in the first internal dimension, compute the noise per unit (noise is stored in unsigned integers whose maximal value, the number of tuples in the hyperplane of the smallest dimension, is assigned to numeric_limits<unsigned int>::max())
  vector<unsigned int>::iterator cardinalityIt = cardinalities.begin();
  unsigned int largestNoise = 1;
  vector<vector<unsigned int>> oldIds2NewIds;
  oldIds2NewIds.reserve(n);
  vector<unsigned int>::const_iterator externalAttributeIdIt = internal2ExternalAttributeOrder.begin();
  vector<Dimension*>::const_iterator dimensionIt = dimensions.begin();
  for (dimensionId = 0; dimensionId != n; ++dimensionId)
    {
      const unsigned int cardinality = (*dimensionIt)->getCardinality();
      delete *dimensionIt++;
      if (dimensionId >= firstSymmetricAttributeId && dimensionId <= lastSymmetricAttributeId)
	{
	  if (dimensionId == firstSymmetricAttributeId)
	    {
	      oldIds2NewIds.insert(oldIds2NewIds.end(), cliqueDimensionsParam.size(), NoisyTuples::createNewIds(cliqueDimensionsParam, hyperplanes, cardinality));
	    }
	}	      
      else
	{
	  if (*externalAttributeIdIt < tauVectorParam.size() && tauVectorParam[*externalAttributeIdIt] != 0)
	    {
	      oldIds2NewIds.push_back(NoisyTuples::createNewIds(hyperplanes[*externalAttributeIdIt], cardinality, noisyTupleFileReader.getIds2Labels(*externalAttributeIdIt)));
	    }
	  else
	    {
	      oldIds2NewIds.push_back(NoisyTuples::createNewIds(hyperplanes[*externalAttributeIdIt], cardinality));
	    }
	}
      if (dimensionId != 0)
	{
	  for (NoisyTuples* hyperplane : hyperplanes[*externalAttributeIdIt])
	    {
	      delete hyperplane;
	    }
	  largestNoise *= cardinality;
	}
      *cardinalityIt++ = cardinality;
      ++externalAttributeIdIt;
    }
  Attribute::noisePerUnit = numeric_limits<unsigned int>::max() / largestNoise;
#ifdef NUMERIC_PRECISION
#ifdef GNUPLOT
      cout << 1. / Attribute::noisePerUnit;
#else
      cout << "Numeric precision: " << 1. / Attribute::noisePerUnit << endl;
#endif
#endif
  // Initialize attributes, minSizes and labels2Ids
  labels2Ids.reserve(n);
  minSizes.reserve(n);
  cardinalityIt = cardinalities.begin();
  vector<vector<unsigned int>>::const_iterator oldIds2NewIdsIt = oldIds2NewIds.begin();
  externalAttributeIdIt = internal2ExternalAttributeOrder.begin();
  attributes.reserve(n);
  for (dimensionId = 0; dimensionId != n; ++dimensionId)
    {
      if (dimensionId == firstSymmetricAttributeId)
	{
	  minSizes.insert(minSizes.end(), cliqueDimensionsParam.size(), minSizeVector[*externalAttributeIdIt]);
	  const vector<string> symmetricLabels = noisyTupleFileReader.setNewIdsAndGetSymmetricLabels(*oldIds2NewIdsIt, *cardinalityIt);
	  cardinalityIt += cliqueDimensionsParam.size();
	  oldIds2NewIdsIt += cliqueDimensionsParam.size();
	  SymmetricAttribute* firstSymmetricAttribute = new SymmetricAttribute(cardinalities, epsilonVector[*externalAttributeIdIt++], symmetricLabels);
	  SymmetricAttribute* secondSymmetricAttribute = new SymmetricAttribute(cardinalities, epsilonVector[*externalAttributeIdIt++], symmetricLabels);
	  firstSymmetricAttribute->setSymmetricAttribute(secondSymmetricAttribute);
	  secondSymmetricAttribute->setSymmetricAttribute(firstSymmetricAttribute);
	  attributes.push_back(firstSymmetricAttribute);
	  attributes.push_back(secondSymmetricAttribute);
	  labels2Ids.insert(labels2Ids.end(), cliqueDimensionsParam.size(), noisyTupleFileReader.captureSymmetricLabels2Ids());
	  ++dimensionId;
	}
      else
	{
	  if (*externalAttributeIdIt < tauVectorParam.size() && tauVectorParam[*externalAttributeIdIt] != 0)
	    {
	      attributes.push_back(new MetricAttribute(cardinalities, epsilonVector[*externalAttributeIdIt], noisyTupleFileReader.setNewIdsAndGetLabels(*externalAttributeIdIt, *oldIds2NewIdsIt++, *cardinalityIt++), tauVectorParam[*externalAttributeIdIt]));
	    }
	  else
	    {
	      attributes.push_back(new Attribute(cardinalities, epsilonVector[*externalAttributeIdIt], noisyTupleFileReader.setNewIdsAndGetLabels(*externalAttributeIdIt, *oldIds2NewIdsIt++, *cardinalityIt++)));
	    }
	  minSizes.push_back(minSizeVector[*externalAttributeIdIt]);
	  labels2Ids.push_back(noisyTupleFileReader.captureLabels2Ids(*externalAttributeIdIt));
	  ++externalAttributeIdIt;
	}
    }
  // Compute order in which to access the tuples in hyperplanes of the first attribute; delete hyperplanes not in the first attribute
  vector<unsigned int> attributeOrderForTuplesInFirstAtributeHyperplanes;
  attributeOrderForTuplesInFirstAtributeHyperplanes.reserve(n - 1);
  const unsigned int firstExternalAttributeId = internal2ExternalAttributeOrder.front();
  const vector<unsigned int>::const_iterator externalAttributeIdEnd = internal2ExternalAttributeOrder.end();
  for (externalAttributeIdIt = internal2ExternalAttributeOrder.begin(); ++externalAttributeIdIt != externalAttributeIdEnd; )
    {
      if (*externalAttributeIdIt < firstExternalAttributeId)
	{
	  attributeOrderForTuplesInFirstAtributeHyperplanes.push_back(*externalAttributeIdIt);
	}
      else
	{
	  if (*externalAttributeIdIt > firstExternalAttributeId)
	    {
	      attributeOrderForTuplesInFirstAtributeHyperplanes.push_back(*externalAttributeIdIt - 1);
	    }
	}
    }
  // Initialize data
  if (isCrisp)
    {
      Trie::setCrisp();
      SparseCrispTube::setDensityThreshold(densityThreshold);
    }
  else
    {
      SparseFuzzyTube::setDensityThreshold(densityThreshold);
    }
  data = new Trie(cardinalities.begin(), cardinalities.end());
  if (lastSymmetricAttributeId != 0)
    {
      // Insert every self loop
      data->setSelfLoops(firstSymmetricAttributeId, lastSymmetricAttributeId, attributes); // WARNING: start with the self loops (no code to insert self loops in dense structures)
    }
  // Insert tuples but self loops
  unsigned int hyperplaneOldId = 0;
  vector<NoisyTuples*>& hyperplanesInFirstAttribute = hyperplanes[firstExternalAttributeId];
  for (NoisyTuples* hyperplane : hyperplanesInFirstAttribute)
    {
      if (!hyperplane->empty())
	{
	  data->setHyperplane(hyperplaneOldId, hyperplane->begin(), hyperplane->end(), attributeOrderForTuplesInFirstAtributeHyperplanes, oldIds2NewIds, attributes);
	}
      delete hyperplane;
      ++hyperplaneOldId;
    }
  // Initialize isClosedVector
  vector<bool> isClosedVector(n, true);
  if (isAgglomeration)
    {
      // Compute Attribute::noisePerUnit * (1 - shiftMultiplier * lambda_0) (see Mirkin's paper)
      Node::setSimilarityShift(shiftMultiplier * (attributes.front()->averagePresentAndPotentialNoise() / largestNoise - Attribute::noisePerUnit));
    }
  else
    {
      unsigned int nbOfUnclosedSymmetricAttribute = 0;
      for (const unsigned int unclosedAttributeId : unclosedDimensions)
	{
	  const unsigned int internalAttributeId = external2InternalAttributeOrder[unclosedAttributeId];
	  if (internalAttributeId < firstSymmetricAttributeId || internalAttributeId > lastSymmetricAttributeId)
	    {
	      isClosedVector[internalAttributeId] = false;
	    }
	  else
	    {
	      ++nbOfUnclosedSymmetricAttribute;
	    }
	}
      if (nbOfUnclosedSymmetricAttribute != 0 && nbOfUnclosedSymmetricAttribute == cliqueDimensionsParam.size())
	{
	  const vector<bool>::iterator end = isClosedVector.begin() + lastSymmetricAttributeId + 1;
	  for (vector<bool>::iterator isClosedVectorIt = isClosedVector.begin() + firstSymmetricAttributeId; isClosedVectorIt != end; ++isClosedVectorIt)
	    {
	      *isClosedVectorIt = false;
	    }
	}
    }
  Attribute::setIsClosedVector(isClosedVector);
#ifdef DETAILED_TIME
  startingPoint = steady_clock::now();
#endif
}

// Constructor of a left subtree
Tree::Tree(const Tree& parent, const vector<Measure*>& mereConstraintsParam): attributes(), mereConstraints(std::move(mereConstraintsParam)), isEnumeratedElementPotentiallyPreventingClosedness(false)
{
#ifdef NB_OF_LEFT_NODES
  ++nbOfLeftNodes;
#endif
  // Deep copy of the attributes
  const vector<Attribute*>& parentAttributes = parent.attributes;
  attributes.reserve(parentAttributes.size());
  vector<unsigned int> sizeOfAttributes;
  sizeOfAttributes.reserve(parentAttributes.size() - 1);
  const vector<Attribute*>::const_iterator parentAttributeEnd = parentAttributes.end();
  for (vector<Attribute*>::const_iterator parentAttributeIt = parentAttributes.begin(); ++parentAttributeIt != parentAttributeEnd; )
    {
      sizeOfAttributes.push_back((*parentAttributeIt)->globalSize());
    }
  const vector<unsigned int>::const_iterator sizeOfAttributesEnd = sizeOfAttributes.end();
  vector<unsigned int>::const_iterator sizeOfAttributeIt = sizeOfAttributes.begin();
  for (vector<Attribute*>::const_iterator parentAttributeIt = parentAttributes.begin(); parentAttributeIt != parentAttributeEnd; ++parentAttributeIt)
    {
      attributes.push_back((*parentAttributeIt)->clone(parentAttributeIt, parentAttributeEnd, sizeOfAttributeIt++, sizeOfAttributesEnd));
    }
  if (firstSymmetricAttributeId != numeric_limits<unsigned int>::max())
    {
      SymmetricAttribute* firstSymmetricAttribute = static_cast<SymmetricAttribute*>(attributes[firstSymmetricAttributeId]);
      SymmetricAttribute* secondSymmetricAttribute = static_cast<SymmetricAttribute*>(attributes[firstSymmetricAttributeId + 1]);
      firstSymmetricAttribute->setSymmetricAttribute(secondSymmetricAttribute);
      secondSymmetricAttribute->setSymmetricAttribute(firstSymmetricAttribute);
    }
}

Tree::~Tree()
{
  for (Attribute* attribute : attributes)
    {
      delete attribute;
    }
  deleteMeasures(mereConstraints);
}

void Tree::initMeasures(const vector<unsigned int>& maxSizesParam, const int maxArea, const vector<string>& groupFileNames, const vector<unsigned int>& groupMinSizesParam, const vector<unsigned int>& groupMaxSizes, const vector<vector<float>>& groupMinRatios, const vector<vector<float>>& groupMinPiatetskyShapiros, const vector<vector<float>>& groupMinLeverages, const vector<vector<float>>& groupMinForces, const vector<vector<float>>& groupMinYulesQs, const vector<vector<float>>& groupMinYulesYs, const char* groupElementSeparator, const char* groupDimensionElementsSeparator, const char* utilityValueFileName, const float minUtility, const char* valueElementSeparator, const char* valueDimensionSeparator, const char* slopePointFileName, const float minSlope, const char* pointElementSeparator, const char* pointDimensionSeparator, const float densityThreshold)
{
  // Helper variables
  const unsigned int n = attributes.size();
  vector<unsigned int> cardinalities;
  cardinalities.reserve(n);
  for (const Attribute* attribute : attributes)
    {
      cardinalities.push_back(attribute->sizeOfPresentAndPotential());
    }
  try
    {
      // Get the maximal sizes in the internal order of the attributes
      vector<unsigned int> maxSizes(cardinalities);
      if (!maxSizesParam.empty())
	{
	  if (maxSizesParam.size() > n)
	    {
	      throw UsageException(("Sizes option should provide at most " + lexical_cast<string>(n) + " sizes!").c_str());
	    }
	  vector<unsigned int>::const_iterator external2InternalAttributeOrderIt = external2InternalAttributeOrder.begin();
	  for (const unsigned int maxSize : maxSizesParam)
	    {
	      maxSizes[*external2InternalAttributeOrderIt] = maxSize;
	      ++external2InternalAttributeOrderIt;
	    }
	  setMinParametersInClique(maxSizes);
	}
      // Initializing measures in increasing cost to update them
      // Initializing MinSize measures
      unsigned int minAreaAccordingToSizes = 1;
      for (unsigned int attributeId = 0; attributeId != n; ++attributeId)
	{
	  const unsigned int minSize = minSizes[attributeId];
	  minAreaAccordingToSizes *= minSize;
	  if (minSize != 0)
	    {
	      mereConstraints.push_back(new MinSize(attributeId, cardinalities[attributeId], minSize));
	    }
	}
      // Initializing minArea measure
      if (minArea > minAreaAccordingToSizes)
	{
	  mereConstraints.push_back(new MinArea(cardinalities, minArea));
	}
      // Initializing MaxSize measures
      for (unsigned int attributeId = 0; attributeId != n; ++attributeId)
	{
	  const unsigned int maxSize = maxSizes[attributeId];
	  if (maxSize < cardinalities[attributeId])
	    {
	      mereConstraints.push_back(new MaxSize(attributeId, maxSize));
	    }
	}
      // Initializing maxArea measure
      if (maxArea != -1)
	{
	  mereConstraints.push_back(new MaxArea(n, maxArea));
	}
      if (!groupFileNames.empty())
	{
	  // Initializing groups
	  GroupMeasure::initGroups(groupFileNames, groupElementSeparator, groupDimensionElementsSeparator, cardinalities, labels2Ids, external2InternalAttributeOrder);
	  // groupMinSizes is to be modified according to the diagonals of MinGroupCoverRatios, MinGroupCoverPiatetskyShapiros, MinGroupCoverLeverages, MinGroupCoverForces, MinGroupCoverYulesQs and  MinGroupCoverYulesYs
	  vector<unsigned int> groupMinSizes = groupMinSizesParam;
	  groupMinSizes.resize(groupFileNames.size());
	  // Initializing MinGroupCoverRatio measures
	  if (!groupMinRatios.empty())
	    {
	      vector<unsigned int>::iterator groupMinSizeIt = groupMinSizes.begin();
	      unsigned int rowId = 0;
	      for (const vector<float>& row : groupMinRatios)
		{
		  if (row.size() > groupFileNames.size())
		    {
		      throw UsageException(("row " + lexical_cast<string>(rowId) + " of file set with gr option provides " + lexical_cast<string>(row.size()) + " ratios but groups option only defines " + lexical_cast<string>(groupFileNames.size()) + " groups!").c_str());
		    }
		  unsigned int columnId = 0;
		  for (const float ratio : row)
		    {
		      if (ratio > 0)
			{
			  if (rowId == columnId)
			    {
			      const unsigned int groupMinSizeAccordingToMatrix = ratio;
			      if (groupMinSizeAccordingToMatrix > *groupMinSizeIt)
				{
				  *groupMinSizeIt = groupMinSizeAccordingToMatrix;
				}
			    }
			  else
			    {
			      mereConstraints.push_back(new MinGroupCoverRatio(rowId, columnId, ratio));
			    }
			}
		      ++columnId;
		    }
		  ++groupMinSizeIt;
		  ++rowId;
		}
	    }
	  // Initializing MinGroupCoverPiatetskyShapiro measures
	  if (!groupMinPiatetskyShapiros.empty())
	    {
	      vector<unsigned int>::iterator groupMinSizeIt = groupMinSizes.begin();
	      unsigned int rowId = 0;
	      for (const vector<float>& row : groupMinPiatetskyShapiros)
		{
		  if (row.size() > groupFileNames.size())
		    {
		      throw UsageException(("row " + lexical_cast<string>(rowId) + " of file set with gps option provides " + lexical_cast<string>(row.size()) + " Piatetsky-Shapiro's measures but groups option only defines " + lexical_cast<string>(groupFileNames.size()) + " groups!").c_str());
		    }
		  unsigned int columnId = 0;
		  for (const float piatetskyShapiro : row)
		    {
		      if (rowId == columnId)
			{
			  const int groupMinSizeAccordingToMatrix = piatetskyShapiro;
			  if (groupMinSizeAccordingToMatrix > static_cast<int>(*groupMinSizeIt))
			    {
			      *groupMinSizeIt = groupMinSizeAccordingToMatrix;
			    }
			}
		      else
			{
			  if (-GroupMeasure::maxCoverOfGroup(rowId) < piatetskyShapiro * GroupMeasure::maxCoverOfGroup(columnId))
			    {
			      mereConstraints.push_back(new MinGroupCoverPiatetskyShapiro(rowId, columnId, piatetskyShapiro));
			    }
			}
		      ++columnId;
		    }
		  ++groupMinSizeIt;
		  ++rowId;
		}
	    }
	  // Initializing MinGroupCoverLeverage measures
	  if (!groupMinLeverages.empty())
	    {
	      vector<unsigned int>::iterator groupMinSizeIt = groupMinSizes.begin();
	      unsigned int rowId = 0;
	      for (const vector<float>& row : groupMinLeverages)
		{
		  if (row.size() > groupFileNames.size())
		    {
		      throw UsageException(("row " + lexical_cast<string>(rowId) + " of file set with gl option provides " + lexical_cast<string>(row.size()) + " leverages but groups option only defines " + lexical_cast<string>(groupFileNames.size()) + " groups!").c_str());
		    }
		  unsigned int columnId = 0;
		  for (const float leverage : row)
		    {
		      if (rowId == columnId)
			{
			  const int groupMinSizeAccordingToMatrix = leverage;
			  if (groupMinSizeAccordingToMatrix > static_cast<int>(*groupMinSizeIt))
			    {
			      *groupMinSizeIt = groupMinSizeAccordingToMatrix;
			    }
			}
		      else
			{
			  if (-GroupMeasure::maxCoverOfGroup(rowId) < leverage * GroupMeasure::maxCoverOfGroup(columnId))
			    {
			      mereConstraints.push_back(new MinGroupCoverLeverage(rowId, columnId, leverage));
			    }
			}
		      ++columnId;
		    }
		  ++groupMinSizeIt;
		  ++rowId;
		}
	    }
	  // Initializing MinGroupCoverForce measures
	  if (!groupMinForces.empty())
	    {
	      vector<unsigned int>::iterator groupMinSizeIt = groupMinSizes.begin();
	      unsigned int rowId = 0;
	      for (const vector<float>& row : groupMinForces)
		{
		  if (row.size() > groupFileNames.size())
		    {
		      throw UsageException(("row " + lexical_cast<string>(rowId) + " of file set with gf option provides " + lexical_cast<string>(row.size()) + " forces but groups option only defines " + lexical_cast<string>(groupFileNames.size()) + " groups!").c_str());
		    }
		  unsigned int columnId = 0;
		  for (const float force : row)
		    {
		      if (force > 0)
			{
			  if (rowId == columnId)
			    {
			      const unsigned int groupMinSizeAccordingToMatrix = force;
			      if (groupMinSizeAccordingToMatrix > *groupMinSizeIt)
				{
				  *groupMinSizeIt = groupMinSizeAccordingToMatrix;
				}
			    }
			  else
			    {
			      mereConstraints.push_back(new MinGroupCoverForce(rowId, columnId, force));
			    }
			}
		      ++columnId;
		    }
		  ++groupMinSizeIt;
		  ++rowId;
		}
	    }
	  // Initializing MinGroupCoverYulesQ measures
	  if (!groupMinYulesQs.empty())
	    {
	      vector<unsigned int>::iterator groupMinSizeIt = groupMinSizes.begin();
	      unsigned int rowId = 0;
	      for (const vector<float>& row : groupMinYulesQs)
		{
		  if (row.size() > groupFileNames.size())
		    {
		      throw UsageException(("row " + lexical_cast<string>(rowId) + " of file set with gyq option provides " + lexical_cast<string>(row.size()) + " Yule's Q measures but groups option only defines " + lexical_cast<string>(groupFileNames.size()) + " groups!").c_str());
		    }
		  unsigned int columnId = 0;
		  for (const float yulesQ : row)
		    {
		      if (rowId == columnId)
			{
			  if (yulesQ > 0)
			    {
			      const unsigned int groupMinSizeAccordingToMatrix = yulesQ;
			      if (groupMinSizeAccordingToMatrix > *groupMinSizeIt)
				{
				  *groupMinSizeIt = groupMinSizeAccordingToMatrix;
				}
			    }
			}
		      else
			{
			  if (yulesQ > -1)
			    {
			      mereConstraints.push_back(new MinGroupCoverYulesQ(rowId, columnId, yulesQ));
			    }
			}
		      ++columnId;
		    }
		  ++groupMinSizeIt;
		  ++rowId;
		}
	    }
	  // Initializing MinGroupCoverYulesY measures
	  if (!groupMinYulesYs.empty())
	    {
	      vector<unsigned int>::iterator groupMinSizeIt = groupMinSizes.begin();
	      unsigned int rowId = 0;
	      for (const vector<float>& row : groupMinYulesYs)
		{
		  if (row.size() > groupFileNames.size())
		    {
		      throw UsageException(("row " + lexical_cast<string>(rowId) + " of file set with gyy option provides " + lexical_cast<string>(row.size()) + " Yule's Y measures but groups option only defines " + lexical_cast<string>(groupFileNames.size()) + " groups!").c_str());
		    }
		  unsigned int columnId = 0;
		  for (const float yulesY : row)
		    {
		      if (rowId == columnId)
			{
			  if (yulesY > 0)
			    {
			      const unsigned int groupMinSizeAccordingToMatrix = yulesY;
			      if (groupMinSizeAccordingToMatrix > *groupMinSizeIt)
				{
				  *groupMinSizeIt = groupMinSizeAccordingToMatrix;
				}
			    }
			}
		      else
			{
			  if (yulesY > -1)
			    {
			      mereConstraints.push_back(new MinGroupCoverYulesY(rowId, columnId, yulesY));
			    }
			}
		      ++columnId;
		    }
		  ++groupMinSizeIt;
		  ++rowId;
		}
	    }
	  // Initializing MinGroupCover measures
	  const vector<unsigned int>::const_iterator end = groupMinSizes.end();
	  vector<unsigned int>::const_iterator groupMinSizeIt = groupMinSizes.begin();
	  for (unsigned int groupId = 0; groupMinSizeIt != end; ++groupId)
	    {
	      if (*groupMinSizeIt != 0)
		{
		  mereConstraints.push_back(new MinGroupCover(groupId, *groupMinSizeIt));
		}
	      ++groupMinSizeIt;
	    }
	  // Initializing MaxGroupCover measures
	  if (!groupMaxSizes.empty())
	    {
	      unsigned int groupId = 0;
	      for (const unsigned int groupMaxSize : groupMaxSizes)
		{
		  if (groupMaxSize < GroupMeasure::maxCoverOfGroup(groupId))
		    {
		      mereConstraints.push_back(new MaxGroupCover(groupId, groupMaxSize));
		    }
		  ++groupId;
		}
	    }
	  if (groupMinSizesParam.empty() && groupMaxSizes.empty() && groupMinRatios.empty() && groupMinPiatetskyShapiros.empty() && groupMinLeverages.empty() && groupMinForces.empty() && groupMinYulesQs.empty() && groupMinYulesYs.empty())
	    {
	      for (unsigned int groupId = 0; groupId != groupFileNames.size(); ++groupId)
		{
		  mereConstraints.push_back(new MinGroupCover(groupId, GroupMeasure::maxCoverOfGroup(groupId)));
		}
	    }
	  GroupMeasure::allMeasuresSet();
	}
      // Initializing min utility measure
      const string utilityValueFileNameString(utilityValueFileName);
      if (!utilityValueFileNameString.empty())
	{
	  mereConstraints.push_back(new MinUtility(utilityValueFileNameString, valueDimensionSeparator, valueElementSeparator, labels2Ids, external2InternalAttributeOrder, cardinalities, densityThreshold, minUtility));
	}
      // Initializing min slope measure
      const string slopePointFileNameString(slopePointFileName);
      if (!slopePointFileNameString.empty())
	{
	  mereConstraints.push_back(new MinSlope(slopePointFileNameString, pointDimensionSeparator, pointElementSeparator, labels2Ids, external2InternalAttributeOrder, cardinalities, densityThreshold, minSlope));
	}
    }
  catch (std::exception& e)
    {
      outputFile.close();
      delete data;
      rethrow_exception(current_exception());
    }
  labels2Ids.clear();
  TupleMeasure::allMeasuresSet(cardinalities);
  stable_partition(mereConstraints.begin(), mereConstraints.end(), monotone);
}

void Tree::mine()
{
  if (attributes.empty())
    {
#ifdef NUMERIC_PRECISION
#ifdef GNUPLOT
      cout << numeric_limits<double>::epsilon();
#else
      cout << "Numeric precision: " << numeric_limits<double>::epsilon() << endl;
#endif
#endif
      return;
    }
  peel();
}

void Tree::terminate(const double maximalNbOfCandidateAgglomerates)
{
  MinUtility::deleteTupleValues();
  MinSlope::deleteTuplePoints();
#ifdef DETAILED_TIME
  const double miningDuration = duration_cast<duration<double>>(steady_clock::now() - startingPoint).count();
  double agglomerationDuration = 0;
#endif
  if (isAgglomeration)
    {
#ifdef DETAILED_TIME
      startingPoint = steady_clock::now();
#endif
      for (pair<list<Node*>::const_iterator, list<Node*>::const_iterator> nodeRange = Node::agglomerateAndSelect(data, maximalNbOfCandidateAgglomerates * 1000000); nodeRange.first != nodeRange.second; ++nodeRange.first)
	{
#ifdef OUTPUT
	  bool isFirst = true;
	  for (const unsigned int internalAttributeId : external2InternalAttributeOrder)
	    {
	      if (isFirst)
		{
		  isFirst = false;
		}
	      else
		{
		  outputFile << outputDimensionSeparator;
		}
	      bool isFirstElement = true;
	      const Attribute& attribute = *attributes[internalAttributeId];
	      for (const unsigned int id : (*nodeRange.first)->dimension(internalAttributeId))
		{
		  if (isFirstElement)
		    {
		      isFirstElement = false;
		    }
		  else
		    {
		      Attribute::printOutputElementSeparator(outputFile);
		    }
		  attribute.printValueFromDataId(id, outputFile);
		}
	    }
	  if (isSizePrinted)
	    {
	      outputFile << patternSizeSeparator;
	      isFirst = true;
	      for (const unsigned int internalAttributeId : external2InternalAttributeOrder)
		{
		  if (isFirst)
		    {
		      isFirst = false;
		    }
		  else
		    {
		      outputFile << sizeSeparator;
		    }
		  outputFile << (*nodeRange.first)->dimension(internalAttributeId).size();
		}
	    }
	  if (isAreaPrinted)
	    {
	      outputFile << sizeAreaSeparator << (*nodeRange.first)->getArea();
	    }
	  outputFile << endl;
#endif
	  delete *nodeRange.first;
	}
#ifdef DETAILED_TIME
      agglomerationDuration = duration_cast<duration<double>>(steady_clock::now() - startingPoint).count();
#endif
    }
  outputFile.close();
  delete data;
#ifdef GNUPLOT
#ifdef NB_OF_CLOSED_N_SETS
#ifdef NUMERIC_PRECISION
  cout << '\t';
#endif
  cout << nbOfClosedNSets;
#endif
#ifdef NB_OF_LEFT_NODES
#if defined NUMERIC_PRECISION || defined NB_OF_CLOSED_N_SETS
  cout << '\t';
#endif
  cout << nbOfLeftNodes;
#endif
#ifdef TIME
#if defined NUMERIC_PRECISION || defined NB_OF_CLOSED_N_SETS || defined NB_OF_LEFT_NODES
  cout << '\t';
#endif
  cout << duration_cast<duration<double>>(steady_clock::now() - overallBeginning).count();
#endif
#ifdef DETAILED_TIME
#if defined NUMERIC_PRECISION || defined NB_OF_CLOSED_N_SETS || defined NB_OF_LEFT_NODES || defined TIME
  cout << '\t';
#endif
  cout << parsingDuration << '\t' << preProcessingDuration << '\t' << miningDuration << '\t' << agglomerationDuration;
#endif
#ifdef MIN_SIZE_ELEMENT_PRUNING_TIME
#if defined NUMERIC_PRECISION || defined NB_OF_CLOSED_N_SETS || defined NB_OF_LEFT_NODES || defined TIME || defined DETAILED_TIME
  cout << '\t';
#endif
  cout << minSizeElementPruningDuration;
#endif
#if defined NUMERIC_PRECISION || defined NB_OF_CLOSED_N_SETS || defined NB_OF_LEFT_NODES || defined TIME || defined DETAILED_TIME || defined MIN_SIZE_ELEMENT_PRUNING_TIME
  cout << endl;
#endif
#else
#ifdef NB_OF_CLOSED_N_SETS
  cout << "Nb of closed ET-" << attributes.size() << "-sets: " << nbOfClosedNSets << endl;
#endif
#ifdef NB_OF_LEFT_NODES
  cout << "Nb of considered " << attributes.size() << "-sets: " << nbOfLeftNodes << endl;
#endif
#ifdef TIME
  cout << "Total time: " << duration_cast<duration<double>>(steady_clock::now() - overallBeginning).count() << 's' << endl;
#endif
#ifdef DETAILED_TIME
  cout << "Parsing time: " << parsingDuration << 's' << endl << "Pre-processing time: " << preProcessingDuration << 's' << endl << "Mining time: " << miningDuration << 's' << endl << "Agglomeration time: " << agglomerationDuration << 's' << endl;
#endif
#ifdef MIN_SIZE_ELEMENT_PRUNING_TIME
  cout << "Time spent identifying min-size irrelevant elements: " << minSizeElementPruningDuration << 's' << endl;
#endif
#endif
}

const bool Tree::leftSubtree(const Attribute& presentAttribute) const
{
  const unsigned int presentAttributeId = presentAttribute.getId();
  vector<Measure*> childMereConstraints = childMeasures(mereConstraints, presentAttributeId, presentAttribute.getChosenValue().getDataId());
  if (childMereConstraints.size() == mereConstraints.size())
    {
      Tree leftChild(*this, childMereConstraints);
      leftChild.setPresent(presentAttributeId);
      return leftChild.isEnumeratedElementPotentiallyPreventingClosedness;
    }
  return true;
}

vector<Measure*> Tree::childMeasures(const vector<Measure*>& parentMeasures, const unsigned int presentAttributeId, const unsigned int presentValueId)
{
  vector<Measure*> childMeasures;
  childMeasures.reserve(parentMeasures.size());
  const vector<unsigned int> elementSetPresent {presentValueId};
  for (const Measure* measure : parentMeasures)
    {
      Measure* childMeasure = measure->clone();
      if (childMeasure->violationAfterAdding(presentAttributeId, elementSetPresent))
	{
	  delete childMeasure;
	  deleteMeasures(childMeasures);
	  return childMeasures;
	}
      childMeasures.push_back(childMeasure);
    }
  // If attribute is symmetric, it always is the first one (given how chosen in peel)
  if (presentAttributeId == firstSymmetricAttributeId)
    {
      for (unsigned int symmetricAttributeId = presentAttributeId + 1; symmetricAttributeId <= lastSymmetricAttributeId; ++symmetricAttributeId)
	{
	  for (Measure* childMeasure : childMeasures)
	    {
	      if (childMeasure->violationAfterAdding(symmetricAttributeId, elementSetPresent))
		{
		  deleteMeasures(childMeasures);
		  childMeasures.clear();
		  return childMeasures;
		}
	    }
	}
    }
  return childMeasures;
}

void Tree::deleteMeasures(vector<Measure*>& measures)
{
  for (Measure* measure : measures)
    {
      delete measure;
    }
}

const bool Tree::monotone(const Measure* measure)
{
  return measure->monotone();
}
  
void Tree::setPresent(const unsigned int presentAttributeId)
{
  const vector<Attribute*>::iterator attributeBegin = attributes.begin();
  const vector<Attribute*>::iterator presentAttributeIt = attributeBegin + presentAttributeId;
  // If attribute is symmetric, it always is the first one (given how chosen in peel)
  if (presentAttributeId == firstSymmetricAttributeId)
    {
      data->setSymmetricPresent(presentAttributeIt, attributeBegin);
    }
  else
    {
      data->setPresent(presentAttributeIt, attributeBegin);
    }
  (*presentAttributeIt)->setChosenValuePresent();
  const vector<Attribute*>::iterator attributeEnd = attributes.end();
  vector<Attribute*>::iterator attributeIt = attributeBegin;
  for (; attributeIt != attributeEnd && !(*attributeIt)->findIrrelevantValuesAndCheckTauContiguity(attributeBegin, attributeEnd) && ((*attributeIt)->irrelevantEmpty() || !violationAfterRemoving((*attributeIt)->getId(), (*attributeIt)->getIrrelevantDataIds())); ++attributeIt)
    {
    }
  if (attributeIt == attributeEnd && !dominated())
    {
      for (Attribute* attribute : attributes)
	{
	  attribute->cleanAbsent(attributeBegin, attributeEnd);
	}
      if (setAbsent())
	{
	  peel();
	}
    }
}

void Tree::rightSubtree(Attribute& absentAttribute, const bool isLastEnumeratedElementPotentiallyPreventingClosedness)
{
  const unsigned int absentAttributeId = absentAttribute.getId();
  const pair<const bool, vector<unsigned int>> tauFarValueDataIds = absentAttribute.tauFarValueDataIdsAndCheckTauContiguity();
  if (!(tauFarValueDataIds.first || violationAfterRemoving(absentAttributeId, tauFarValueDataIds.second) || dominated()))
    {
      const vector<Attribute*>::iterator attributeBegin = attributes.begin();
      const vector<Attribute*>::iterator absentAttributeIt = attributeBegin + absentAttributeId;
      // If attribute is symmetric, it always is the first one (given how chosen in peel)
      if (absentAttributeId == firstSymmetricAttributeId)
	{
	  data->setSymmetricAbsent(absentAttributeIt, attributeBegin);
	}
      else
	{
	  data->setAbsent(absentAttributeIt, tauFarValueDataIds.second, attributeBegin);
	}
      (*absentAttributeIt)->setChosenValueAbsent(isLastEnumeratedElementPotentiallyPreventingClosedness);
#ifdef MIN_SIZE_ELEMENT_PRUNING
      if (findMinSizeIrrelevantValuesAndCheckConstraints(absentAttributeIt) && setAbsent())
	{
	  peel();
	}
#else
      peel();
#endif
    }
}

const bool Tree::setAbsent()
{
  const vector<Attribute*>::iterator attributeBegin = attributes.begin();
  const vector<Attribute*>::iterator attributeToPurgeIt = max_element(attributeBegin, attributes.end(), Attribute::lessAppealingIrrelevant);
  if ((*attributeToPurgeIt)->irrelevantEmpty())
    {
      return true;
    }
  data->setAbsent(attributeToPurgeIt, (*attributeToPurgeIt)->eraseIrrelevantValues(), attributeBegin);
#ifdef MIN_SIZE_ELEMENT_PRUNING
  return findMinSizeIrrelevantValuesAndCheckConstraints(attributeToPurgeIt) && setAbsent();
#else
  return setAbsent();
#endif
}

#ifdef MIN_SIZE_ELEMENT_PRUNING
const bool Tree::findMinSizeIrrelevantValuesAndCheckConstraints(const vector<Attribute*>::iterator previousAbsentAttributeIt)
{
#ifdef MIN_SIZE_ELEMENT_PRUNING_TIME
  const steady_clock::time_point startingPoint = steady_clock::now();
#endif
  const vector<unsigned int> thresholds = minSizeIrrelevancyThresholds();
  const vector<Attribute*>::iterator attributeEnd = attributes.end();
  vector<Attribute*>::iterator attributeIt = attributes.begin();
  for (; attributeIt != attributeEnd && !((attributeIt != previousAbsentAttributeIt || dynamic_cast<SymmetricAttribute*>(*attributeIt)) && (*attributeIt)->presentAndPotentialIrrelevant(thresholds[(*attributeIt)->getId()])); ++attributeIt)
    {
    }
  if (attributeIt != attributeEnd)
    {
#ifdef MIN_SIZE_ELEMENT_PRUNING_TIME
      minSizeElementPruningDuration += duration_cast<duration<double>>(steady_clock::now() - startingPoint).count();
#endif
      return false;
    }
  for (Attribute* attribute : attributes)
    {
      const unsigned int attributeId = attribute->getId();
      if (attributeId < firstSymmetricAttributeId || attributeId > lastSymmetricAttributeId)
  	{
  	  if (attribute != *previousAbsentAttributeIt)
	    {
	      const unsigned int threshold = thresholds[attributeId];
	      const pair<bool, vector<unsigned int>> isViolatingTauContiguityAndNewIrrelevantValues = attribute->findPresentAndPotentialIrrelevantValuesAndCheckTauContiguity(threshold);
	      if (isViolatingTauContiguityAndNewIrrelevantValues.first || (!isViolatingTauContiguityAndNewIrrelevantValues.second.empty() && violationAfterRemoving(attributeId, isViolatingTauContiguityAndNewIrrelevantValues.second)))
		{
#ifdef MIN_SIZE_ELEMENT_PRUNING_TIME
		  minSizeElementPruningDuration += duration_cast<duration<double>>(steady_clock::now() - startingPoint).count();
#endif
		  return false;
		}
	      attribute->presentAndPotentialCleanAbsent(threshold);
	    }
  	}
      else
  	{
	  const unsigned int threshold = thresholds[attributeId];
	  vector<unsigned int> newIrrelevantValues = attribute->findPresentAndPotentialIrrelevantValuesAndCheckTauContiguity(threshold).second;
	  if (!newIrrelevantValues.empty())
	    {
	      unsigned int symmetricAttributeId = firstSymmetricAttributeId;
	      for (; symmetricAttributeId <= lastSymmetricAttributeId && !violationAfterRemoving(symmetricAttributeId, newIrrelevantValues); ++symmetricAttributeId)
		{
		}
	      if (symmetricAttributeId <= lastSymmetricAttributeId)
		{
#ifdef MIN_SIZE_ELEMENT_PRUNING_TIME
		  minSizeElementPruningDuration += duration_cast<duration<double>>(steady_clock::now() - startingPoint).count();
#endif
		  return false;
		}
	    }
	  attribute->presentAndPotentialCleanAbsent(threshold);
	}
    }
#ifdef MIN_SIZE_ELEMENT_PRUNING_TIME
  minSizeElementPruningDuration += duration_cast<duration<double>>(steady_clock::now() - startingPoint).count();
#endif
  return !dominated();
}

vector<unsigned int> Tree::minSizeIrrelevancyThresholds() const
{
  const unsigned int n = attributes.size();
  // Compute the minimal and the maximal sizes of a pattern, the area of the minimal pattern and the minimal/maximal number of symmetric elements
  vector<unsigned int> maximalPatternSizes;
  maximalPatternSizes.reserve(n);
  vector<unsigned int> minimalPatternSizes;
  minimalPatternSizes.reserve(n);
  vector<unsigned int>::const_iterator minSizeIt = minSizes.begin();
  if (firstSymmetricAttributeId == numeric_limits<unsigned int>::max())
    {
      for (const Attribute* attribute : attributes)
	{
	  minimalPatternSizes.push_back(max(*minSizeIt++, attribute->sizeOfPresent()));
	  maximalPatternSizes.push_back(attribute->sizeOfPresentAndPotential());
	}
      // Sum the epsilons with the number of non-self-loops tuples in the maximal pattern and subtract the number of non-self-loops tuples in the minimal pattern
      vector<unsigned int> thresholds(Attribute::getEpsilonVector());
      vector<unsigned int>::iterator thresholdIt = thresholds.begin();
      vector<Attribute*>::const_iterator attributeIt = attributes.begin();
      for (unsigned int dimensionId = 0; dimensionId != n; ++dimensionId)
	{
	  // TODO: check whether round-off errors are problematic (round instead of ceil?)
	  *thresholdIt++ += Attribute::noisePerUnit * (IndistinctSkyPatterns::nbOfNonSelfLoopTuplesInHyperplaneOfPattern(maximalPatternSizes, dimensionId, 0) - max(static_cast<double>(IndistinctSkyPatterns::nbOfNonSelfLoopTuplesInHyperplaneOfPattern(minimalPatternSizes, dimensionId, 0)), ceil(minArea / (*attributeIt)->sizeOfPresentAndPotential())));
	  ++attributeIt;
	}
      return thresholds;
    }
  unsigned int maxNbOfSymmetricElements = numeric_limits<unsigned int>::max();
  unsigned int minNbOfSymmetricElements = numeric_limits<unsigned int>::max();
  double minNbOfSymmetricElementsAccordingToArea = minArea;
  unsigned int maxAreaIgnoringSymmetricAttributes = 1;
  vector<Attribute*>::const_iterator attributeIt = attributes.begin();
  for (unsigned int dimensionId = 0; dimensionId != n; ++dimensionId)
    {
      unsigned int size = max(*minSizeIt, (*attributeIt)->sizeOfPresent());
      minimalPatternSizes.push_back(size);
      size = (*attributeIt)->sizeOfPresentAndPotential();
      if (dimensionId < firstSymmetricAttributeId || dimensionId > lastSymmetricAttributeId)
	{
	  maxAreaIgnoringSymmetricAttributes *= size;
	  minNbOfSymmetricElementsAccordingToArea /= size;
	}
      else
	{
	  if (minimalPatternSizes.back() < minNbOfSymmetricElements)
	    {
	      minNbOfSymmetricElements = minimalPatternSizes.back();
	    }
	  if (size < maxNbOfSymmetricElements)
	    {
	      maxNbOfSymmetricElements = size;
	    }
	}
      maximalPatternSizes.push_back(size);
      ++attributeIt;
      ++minSizeIt;
    }
  // TODO: check whether round-off errors are problematic (round instead of ceil?)
  minNbOfSymmetricElementsAccordingToArea = ceil(pow(minNbOfSymmetricElementsAccordingToArea, 1. / (lastSymmetricAttributeId - firstSymmetricAttributeId + 1)));
  if (static_cast<unsigned int>(minNbOfSymmetricElementsAccordingToArea) > minNbOfSymmetricElements)
    {
      minNbOfSymmetricElements = minNbOfSymmetricElementsAccordingToArea;
    }
  vector<unsigned int>::iterator minimalPatternSizeIt = minimalPatternSizes.begin() + firstSymmetricAttributeId;
  for (unsigned int dimensionId = firstSymmetricAttributeId; dimensionId <= lastSymmetricAttributeId; ++dimensionId)
    {
      *minimalPatternSizeIt++ = minNbOfSymmetricElements;
    }
  // Sum the epsilons with the number of non-self-loops tuples in the maximal pattern and subtract the number of non-self-loops tuples in the minimal pattern
  vector<unsigned int> thresholds(Attribute::getEpsilonVector());
  vector<unsigned int>::iterator thresholdIt = thresholds.begin();
  attributeIt = attributes.begin();
  for (unsigned int dimensionId = 0; dimensionId != n; ++dimensionId)
    {
      if (dimensionId < firstSymmetricAttributeId || dimensionId > lastSymmetricAttributeId)
	{
	  // TODO: check whether round-off errors are problematic (round instead of ceil?)
	  *thresholdIt++ += Attribute::noisePerUnit * (IndistinctSkyPatterns::nbOfNonSelfLoopTuplesInHyperplaneOfPattern(maximalPatternSizes, dimensionId, minNbOfSymmetricElements) - max(static_cast<double>(IndistinctSkyPatterns::nbOfNonSelfLoopTuplesInHyperplaneOfPattern(minimalPatternSizes, dimensionId, minNbOfSymmetricElements)), ceil((minArea - maxAreaIgnoringSymmetricAttributes * maxNbOfSymmetricElements) / (*attributeIt)->sizeOfPresentAndPotential())));
	}
      else
	{
	  // TODO: check whether round-off errors are problematic (round instead of ceil?)
	  *thresholdIt++ += Attribute::noisePerUnit * (IndistinctSkyPatterns::nbOfNonSelfLoopTuplesInHyperplaneOfPattern(maximalPatternSizes, dimensionId, minNbOfSymmetricElements) - max(static_cast<double>(IndistinctSkyPatterns::nbOfNonSelfLoopTuplesInHyperplaneOfPattern(minimalPatternSizes, dimensionId, minNbOfSymmetricElements)), ceil(minArea / (*attributeIt)->sizeOfPresentAndPotential() - maxAreaIgnoringSymmetricAttributes)));
	}
      ++attributeIt;
    }
  return thresholds;
}
#endif

const bool Tree::violationAfterAdding(const unsigned int dimensionIdOfElementsSetPresent, const vector<unsigned int>& elementsSetPresent)
{
  for (Measure* measure : mereConstraints)
    {
      if (measure->violationAfterAdding(dimensionIdOfElementsSetPresent, elementsSetPresent))
	{
	  isEnumeratedElementPotentiallyPreventingClosedness = true;
	  return true;
	}
    }
  return false;
}

const bool Tree::violationAfterRemoving(const unsigned int dimensionIdOfElementsSetAbsent, const vector<unsigned int>& elementsSetAbsent)
{
  for (Measure* measure : mereConstraints)
    {
      if (measure->violationAfterRemoving(dimensionIdOfElementsSetAbsent, elementsSetAbsent))
	{
	  if (!measure->monotone())
	    {
	      isEnumeratedElementPotentiallyPreventingClosedness = true;
	    }
	  return true;
	}
    }
  return false;
}

const bool Tree::dominated()
{
  return false;
}

void Tree::validPattern() const
{
  if (isAgglomeration)
    {
      new Node(attributes);
    }
#ifdef OUTPUT
  else
    {
      outputFile << *this;
      if (isSizePrinted)
	{
	  outputFile << patternSizeSeparator;
	  bool isFirstSize = true;
	  for (const unsigned int internalAttributeId : external2InternalAttributeOrder)
	    {
	      if (isFirstSize)
		{
		  isFirstSize = false;
		}
	      else
		{
		  outputFile << sizeSeparator;
		}
	      outputFile << attributes[internalAttributeId]->sizeOfPresent();
	    }
	}
      if (isAreaPrinted)
	{
	  unsigned int area = 1;
	  for (const Attribute* attribute : attributes)
	    {
	      area *= attribute->sizeOfPresent();
	    }
	  outputFile << sizeAreaSeparator << area;
	}
      outputFile << endl;
    }
#endif
}

void Tree::setMinParametersInClique(vector<unsigned int>& parameterVector)
{
  if (firstSymmetricAttributeId != numeric_limits<unsigned int>::max())
    {
      unsigned int min = numeric_limits<unsigned int>::max();
      vector<unsigned int>::iterator parameterIt = parameterVector.begin() + firstSymmetricAttributeId;
      for (unsigned int symmetricAttributeId = firstSymmetricAttributeId; symmetricAttributeId <= lastSymmetricAttributeId; ++symmetricAttributeId)
	{
	  if (*parameterIt < min)
	    {
	      min = *parameterIt;
	    }
	  ++parameterIt;
	}
      for (unsigned int symmetricAttributeId = firstSymmetricAttributeId; symmetricAttributeId <= lastSymmetricAttributeId; ++symmetricAttributeId)
	{
	  *(--parameterIt) = min;
	}
    }
}

void Tree::setMaxParametersInClique(vector<unsigned int>& parameterVector)
{
  if (firstSymmetricAttributeId != numeric_limits<unsigned int>::max())
    {
      unsigned int max = 0;
      vector<unsigned int>::iterator parameterIt = parameterVector.begin() + firstSymmetricAttributeId;
      for (unsigned int symmetricAttributeId = firstSymmetricAttributeId; symmetricAttributeId <= lastSymmetricAttributeId; ++symmetricAttributeId)
	{
	  if (*parameterIt > max)
	    {
	      max = *parameterIt;
	    }
	  ++parameterIt;
	}
      for (unsigned int symmetricAttributeId = firstSymmetricAttributeId; symmetricAttributeId <= lastSymmetricAttributeId; ++symmetricAttributeId)
	{
	  *(--parameterIt) = max;
	}
    }
}

void Tree::peel()
{
  const vector<Attribute*>::iterator attributeBegin = attributes.begin();
  const vector<Attribute*>::iterator attributeEnd = attributes.end();
#ifdef ASSERT
  for (vector<Attribute*>::const_iterator attributeIt = attributeBegin; attributeIt != attributeEnd; ++attributeIt)
    {
      const vector<Value*>::const_iterator end = (*attributeIt)->absentEnd();
      for (vector<Value*>::const_iterator valueIt = (*attributeIt)->presentBegin(); valueIt != end; ++valueIt)
	{
	  unsigned int actualNoise = data->countNoiseOnPresent(attributeIt, **valueIt, attributeBegin);
	  unsigned int pretendedNoise = (*valueIt)->getPresentNoise();
	  if (actualNoise != pretendedNoise)
	    {
	      (*attributeIt)->printValue(**valueIt, cerr);
	      cerr << " has " << static_cast<double>(actualNoise) / Attribute::noisePerUnit << " present noise and pretends to have " << static_cast<double>(pretendedNoise) / Attribute::noisePerUnit << endl;
	    }
	  actualNoise = data->countNoiseOnPresentAndPotential(attributeIt, **valueIt, attributeBegin);
	  pretendedNoise = (*valueIt)->getPresentAndPotentialNoise();
	  if (actualNoise != pretendedNoise)
	    {
	      (*attributeIt)->printValue(**valueIt, cerr);
	      cerr << " has " << static_cast<double>(actualNoise) / Attribute::noisePerUnit << " present and potential noise and pretends to have " << static_cast<double>(pretendedNoise) / Attribute::noisePerUnit << endl;
	    }
	}
    }
#endif
  bool unclosedInMetricAttribute = false;
  for (const Attribute* attribute : attributes)
    {
      if (attribute->unclosed(attributeBegin, attributeEnd))
	{
	  if (!dynamic_cast<const MetricAttribute*>(attribute))
	    {
	      return;
	    }
	  unclosedInMetricAttribute = true;
	}
    }
  if (unclosedInMetricAttribute)
    {
      isEnumeratedElementPotentiallyPreventingClosedness = true;
      return;
    }
  vector<Attribute*>::iterator attributeIt = attributeBegin;
  for (; attributeIt != attributeEnd && (*attributeIt)->finalizable(); ++attributeIt)
    {
    }
  if (attributeIt == attributeEnd)
    {
      // Leaf
#ifdef DEBUG
      cout << "Every remaining potential value is present!" << endl;
#endif
      unsigned int attributeId = 0;
      for (Attribute* attribute : attributes)
	{
	  const vector<unsigned int> elementsSetPresent = attribute->finalize();
	  if (!elementsSetPresent.empty() && violationAfterAdding(attributeId, elementsSetPresent))
	    {
	      return;
	    }
	  ++attributeId;
	}
      if (dominated())
	{
	  return;
	}
    }
#ifdef DEBUG
  cout << "Node after:" << endl;
  printNode(cout);
  cout << endl;
#endif
  attributeIt = attributeBegin;
  for (attributeIt = attributeBegin; attributeIt != attributeEnd && (*attributeIt)->potentialEmpty(); ++attributeIt)
    {
    }
  if (attributeIt == attributeEnd) // cannot be integrated to the same last test because of rounding errors
    {
#ifdef DEBUG
      cout << "*********************** closed ET-" << attributes.size() << "-set ************************" << endl << *this << endl << "****************************************************************" << endl;
#endif
#ifdef NB_OF_CLOSED_N_SETS
      ++nbOfClosedNSets;
#endif
      validPattern();
      isEnumeratedElementPotentiallyPreventingClosedness = true;
      return;
    }
  // Not a leaf
  // Choose the next value to peel
  vector<Attribute*>::iterator attributeToPeelIt = attributeIt;
  double maxAppeal = (*attributeIt)->getAppeal(attributeBegin, attributeEnd);
  while (++attributeIt != attributeEnd)
    {
      if (!(*attributeIt)->potentialEmpty())
	{
	  double appeal = (*attributeIt)->getAppeal(attributeBegin, attributeEnd);
	  if (appeal > maxAppeal)
	    {
	      attributeToPeelIt = attributeIt;
	      maxAppeal = appeal;
	    }
	}
    }
  (*attributeToPeelIt)->chooseValue();
  // Construct the subtrees
#ifdef DEBUG
  const Value& enumeratedValue = (*attributeToPeelIt)->getChosenValue();
  cout << "Left child: ";
  (*attributeToPeelIt)->printValue(enumeratedValue, cout);
  cout << " set present" << endl << "Node before:" << endl;
  printNode(cout);
  cout << endl;
#endif
  const bool isLastEnumeratedElementPotentiallyPreventingClosedness = leftSubtree(**attributeToPeelIt);
#ifdef DEBUG
  cout << "Right child: ";
  (*attributeToPeelIt)->printValue(enumeratedValue, cout);
  cout << " set absent";
  if (!isLastEnumeratedElementPotentiallyPreventingClosedness)
    {
      cout << " and cannot prevent the closedness of any future pattern";
    }
  cout << endl << "Node before:" << endl;
  printNode(cout);
  cout << endl;
#endif
  rightSubtree(**attributeToPeelIt, isLastEnumeratedElementPotentiallyPreventingClosedness);
  if (isLastEnumeratedElementPotentiallyPreventingClosedness)
    {
      isEnumeratedElementPotentiallyPreventingClosedness = true;
    }
}

ostream& operator<<(ostream& out, const Tree& node)
{
  bool isFirstAttribute = true;
  for (const unsigned int internalAttributeId : node.external2InternalAttributeOrder)
    {
      if (isFirstAttribute)
	{
	  isFirstAttribute = false;
	}
      else
	{
	  out << Tree::outputDimensionSeparator;
	}
      out << *(node.attributes[internalAttributeId]);
    }
  return out;
}

#ifdef DEBUG
void Tree::printNode(ostream& out) const
{
  out << "  present: ";
  bool isFirstAttribute = true;
  for (const unsigned int internalAttributeId : external2InternalAttributeOrder)
    {
      if (isFirstAttribute)
	{
	  isFirstAttribute = false;
	}
      else
	{
	  out << outputDimensionSeparator;
	}
      attributes[internalAttributeId]->printPresent(out);
    }
  out << endl << "  potential: ";
  isFirstAttribute = true;
  for (const unsigned int internalAttributeId : external2InternalAttributeOrder)
    {
      if (isFirstAttribute)
	{
	  isFirstAttribute = false;
	}
      else
	{
	  out << outputDimensionSeparator;
	}
      attributes[internalAttributeId]->printPotential(out);
    }
  out << endl << "  possible extensions: ";
  isFirstAttribute = true;
  for (const unsigned int internalAttributeId : external2InternalAttributeOrder)
    {
      if (isFirstAttribute)
	{
	  isFirstAttribute = false;
	}
      else
	{
	  out << outputDimensionSeparator;
	}
      attributes[internalAttributeId]->printAbsent(out);
    }
}
#endif
