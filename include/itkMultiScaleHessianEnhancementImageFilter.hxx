/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
#ifndef itkMultiScaleHessianEnhancementImageFilter_hxx
#define itkMultiScaleHessianEnhancementImageFilter_hxx

#include "itkMultiScaleHessianEnhancementImageFilter.h"
#include "itkMath.h"
#include "itkProgressAccumulator.h"
#include "itkExceptionObject.h"

namespace itk
{
template< typename TInputImage, typename TOutputImage >
MultiScaleHessianEnhancementImageFilter< TInputImage, TOutputImage >
::MultiScaleHessianEnhancementImageFilter()
{
  /* Sigma member variables */
  m_SigmaArray.SetSize(0);

  /* Instantiate filters. */
  m_HessianFilter               = HessianFilterType::New();
  m_EigenAnalysisFilter         = EigenAnalysisFilterType::New();
  m_MaximumAbsoluteValueFilter  = MaximumAbsoluteValueFilterType::New();
  m_EigenToScalarImageFilter    = nullptr; // has to be provided by the user.

  /* We require an input image */
  this->SetNumberOfRequiredInputs( 1 );
}

template< typename TInputImage, typename TOutputImage >
void
MultiScaleHessianEnhancementImageFilter< TInputImage, TOutputImage >
::GenerateInputRequestedRegion()
{
  Superclass::GenerateInputRequestedRegion();
  if ( this->GetInput() )
  {
  typename TInputImage::Pointer image =
    const_cast< TInputImage * >( this->GetInput() );
  image->SetRequestedRegionToLargestPossibleRegion();
  }
}

template< typename TInputImage, typename TOutputImage >
void
MultiScaleHessianEnhancementImageFilter< TInputImage, TOutputImage >
::EnlargeOutputRequestedRegion(DataObject *data)
{
  Superclass::EnlargeOutputRequestedRegion(data);
  data->SetRequestedRegionToLargestPossibleRegion();
}

template< typename TInputImage, typename TOutputImage >
void
MultiScaleHessianEnhancementImageFilter< TInputImage, TOutputImage >
::GenerateData()
{
  /* Test all inputs are set */
  if ( !m_EigenToScalarImageFilter )
  {
    itkExceptionMacro(<< "EigenToScalarImageFilter is not present");
  }

  if ( m_SigmaArray.GetSize() < 1 )
  {
    itkExceptionMacro(<< "SigmaArray must have at least one sigma value. Given array of size " << m_SigmaArray.GetSize());
  }

  /* Set filters parameters */
  m_HessianFilter->SetNormalizeAcrossScale(true);
  m_EigenAnalysisFilter->SetDimension(ImageDimension);
  m_EigenAnalysisFilter->OrderEigenValuesBy(this->ConvertType(m_EigenToScalarImageFilter->GetEigenValueOrder()));

  /* Connect filters */
  m_HessianFilter->SetInput(this->GetInput());
  m_EigenAnalysisFilter->SetInput(m_HessianFilter->GetOutput());
  m_EigenToScalarImageFilter->SetInput(m_EigenAnalysisFilter->GetOutput());

  /* After executing we want to release data to save memory */
  m_HessianFilter->ReleaseDataFlagOn();
  m_EigenAnalysisFilter->ReleaseDataFlagOn();
  m_EigenToScalarImageFilter->ReleaseDataFlagOn();

  /* Setup progress reporter */
  ProgressAccumulator::Pointer progress = ProgressAccumulator::New();
  progress->SetMiniPipelineFilter(this);

  /*
   * We checked that m_SigmaArray.GetSize() > 0 above and do not need to repeat the check. However,
   * if we are only given one sigma value we do not need to take the maximum over scales.
   * 
   * Three filters, ran m_SigmaArray.GetSize() times
   * One filter, ran (m_SigmaArray.GetSize() - 1) times
   */
  float numberOfFiltersToProcess = 3*m_SigmaArray.GetSize() + 1*(m_SigmaArray.GetSize()-1);
  float perFilterProccessPercentage = 1.0 / numberOfFiltersToProcess;
  itkDebugMacro(<< "each filter accounts for " << perFilterProccessPercentage*100.0 << "% of processing");

  progress->RegisterInternalFilter(m_HessianFilter, m_SigmaArray.GetSize()*perFilterProccessPercentage);
  progress->RegisterInternalFilter(m_EigenAnalysisFilter, m_SigmaArray.GetSize()*perFilterProccessPercentage);
  progress->RegisterInternalFilter(m_EigenToScalarImageFilter, m_SigmaArray.GetSize()*perFilterProccessPercentage);

  /* Check if we need to run the MaximumAbsoluteValueFilter at all */ 
  if (m_SigmaArray.GetSize() > 1)
  {
    progress->RegisterInternalFilter(m_MaximumAbsoluteValueFilter, (m_SigmaArray.GetSize()-1)*perFilterProccessPercentage);
  }
  else
  {
    itkDebugMacro(<< "maximumAbsoluteValueFilter is not being used");
  }

  /* We store a single pointer that we will graft to the output */
  typename TOutputImage::Pointer outputImageTypePointer;

  /* Process the first scale */
  outputImageTypePointer = generateResponseAtScale((SigmaStepsType)0);

  /* Process the remaining sigma values */
  for (SigmaStepsType scaleLevel = 1; scaleLevel < m_SigmaArray.GetSize(); ++scaleLevel)
  {
    /* Calculate next response value */
    typename TOutputImage::Pointer tempResponseOutputImageTypePointer = generateResponseAtScale(scaleLevel);

    /* Take absolute value maximum */
    m_MaximumAbsoluteValueFilter->SetInput1(outputImageTypePointer);
    m_MaximumAbsoluteValueFilter->SetInput2(tempResponseOutputImageTypePointer);
    m_MaximumAbsoluteValueFilter->Update();

    /* Save max and go to next sigma value */
    outputImageTypePointer = m_MaximumAbsoluteValueFilter->GetOutput();
  }

  /* Graft output and we're done! */
  this->GraftOutput(outputImageTypePointer);
}

template< typename TInputImage, typename TOutputImage >
typename TOutputImage::Pointer
MultiScaleHessianEnhancementImageFilter< TInputImage, TOutputImage >
::generateResponseAtScale(SigmaStepsType scaleLevel)
{
  /* Get this sigma value */
  SigmaType thisSigma = m_SigmaArray.GetElement(scaleLevel);

  /* Process pipeline and return */
  m_HessianFilter->SetSigma(thisSigma);
  m_EigenToScalarImageFilter->Update();
  return m_EigenToScalarImageFilter->GetOutput();
}

template< typename TInputImage, typename TOutputImage >
typename MultiScaleHessianEnhancementImageFilter< TInputImage, TOutputImage >::SigmaArrayType
MultiScaleHessianEnhancementImageFilter< TInputImage, TOutputImage >
::GenerateSigmaArray(SigmaType SigmaMinimum, SigmaType SigmaMaximum, SigmaStepsType NumberOfSigmaSteps, SigmaStepMethodEnum SigmaStepMethod)
{
  /* Quick check to make sure value is correct */
  if ( NumberOfSigmaSteps < 1 )
  {
    throw ExceptionObject(__FILE__, __LINE__, "Number of sigma values requested is less than 1", ITK_LOCATION);
  }

  /* If we have min greater than max just swap */
  if (SigmaMinimum > SigmaMaximum) {
    std::swap(SigmaMinimum, SigmaMaximum);
  }

  /* If min equals max, just generate one step size */
  if (SigmaMinimum == SigmaMaximum) {
    NumberOfSigmaSteps = 1;
  }

  /* Create array and resize */
  SigmaArrayType sigmaArray;
  sigmaArray.SetSize(NumberOfSigmaSteps);

  /* Populate first element to avoid division by zero */
  sigmaArray.SetElement(0, SigmaMinimum);

  /* Populate the remaining array */
  SigmaType thisSigma;
  for (SigmaStepsType scaleLevel = 1; scaleLevel < sigmaArray.GetSize(); ++scaleLevel)
  {
    switch ( SigmaStepMethod )
    {
    case Self::EquispacedSigmaSteps:
      {
      const RealType stepSize = std::max( 1e-10, ( SigmaMaximum - SigmaMinimum ) / ( NumberOfSigmaSteps - 1 ) );
      thisSigma = SigmaMinimum + stepSize * scaleLevel;
      break;
      }
    case Self::LogarithmicSigmaSteps:
      {
      const RealType stepSize =
        std::max( 1e-10, ( std::log(SigmaMaximum) - std::log(SigmaMinimum) ) / ( NumberOfSigmaSteps - 1 ) );
      thisSigma = std::exp(std::log (SigmaMinimum) + stepSize * scaleLevel);
      break;
      }
    default:
      throw ExceptionObject(__FILE__, __LINE__, "Requested sigma step method does not exist", ITK_LOCATION);
    }
    
    /* Assign to array */
    sigmaArray.SetElement(scaleLevel, thisSigma);
  }

  return sigmaArray;
}

template< typename TInputImage, typename TOutputImage >
typename MultiScaleHessianEnhancementImageFilter< TInputImage, TOutputImage >::SigmaArrayType
MultiScaleHessianEnhancementImageFilter< TInputImage, TOutputImage >
::GenerateEquispacedSigmaArray(SigmaType SigmaMinimum, SigmaType SigmaMaximum, SigmaStepsType NumberOfSigmaSteps)
{
  return GenerateSigmaArray(SigmaMinimum, SigmaMaximum, NumberOfSigmaSteps, Self::EquispacedSigmaSteps);
}

template< typename TInputImage, typename TOutputImage >
typename MultiScaleHessianEnhancementImageFilter< TInputImage, TOutputImage >::SigmaArrayType
MultiScaleHessianEnhancementImageFilter< TInputImage, TOutputImage >
::GenerateLogarithmicSigmaArray(SigmaType SigmaMinimum, SigmaType SigmaMaximum, SigmaStepsType NumberOfSigmaSteps)
{
  return GenerateSigmaArray(SigmaMinimum, SigmaMaximum, NumberOfSigmaSteps, Self::LogarithmicSigmaSteps);
}

template< typename TInputImage, typename TOutputImage >
typename MultiScaleHessianEnhancementImageFilter< TInputImage, TOutputImage >::InternalEigenValueOrderType
MultiScaleHessianEnhancementImageFilter< TInputImage, TOutputImage >
::ConvertType(ExternalEigenValueOrderType order)
{
  switch(order)
  {
  case EigenToScalarImageFilterType::OrderByValue:
    return EigenAnalysisFilterType::FunctorType::OrderByValue;
  case EigenToScalarImageFilterType::OrderByMagnitude:
    return EigenAnalysisFilterType::FunctorType::OrderByMagnitude;
  case EigenToScalarImageFilterType::DoNotOrder:
    return EigenAnalysisFilterType::FunctorType::DoNotOrder;
  default:
    itkExceptionMacro(<< "Trying to convert bad order " << order);
  }
}

template< typename TInputImage, typename TOutputImage >
void
MultiScaleHessianEnhancementImageFilter< TInputImage, TOutputImage >
::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);
  os << indent << "HessianFilter: " << m_HessianFilter.GetPointer() << std::endl;
  os << indent << "EigenAnalysisFilter: " << m_EigenAnalysisFilter.GetPointer() << std::endl;
  os << indent << "MaximumAbsoluteValueFilter: " << m_MaximumAbsoluteValueFilter.GetPointer() << std::endl;
  os << indent << "EigenToScalarImageFilter: " << m_EigenToScalarImageFilter.GetPointer() << std::endl;
  os << indent << "SigmaArray: " << m_SigmaArray << std::endl;
}

} // end namespace itk

#endif // itkMultiScaleHessianEnhancementImageFilter_hxx
