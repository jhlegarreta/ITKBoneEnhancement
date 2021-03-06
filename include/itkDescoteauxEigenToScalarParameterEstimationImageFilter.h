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

#ifndef itkDescoteauxEigenToScalarParameterEstimationImageFilter_h
#define itkDescoteauxEigenToScalarParameterEstimationImageFilter_h

#include "itkImageToImageFilter.h"
#include "itkNumericTraits.h"
#include "itkSimpleDataObjectDecorator.h"
#include "itkArray.h"

namespace itk {
/** \class DescoteauxEigenToScalarParameterEstimationImageFilter
 * \brief Automatic parameter estimation for the Krcah method
 * 
 * The default parameters are:
 *   \f{eqnarray*}{
 *      \alpha &=& 0.5 \\
 *      \beta &=& 0.5 \\
 *      \gamma &=& 0.5 max\( Frobenius norm \)
 *   \f}
 * 
 * Where the Frobenius norm for a real, symmetric matrix is given by
 * the square root of the sum of squares of the eigenvalues.
 * 
 * If the input image and mask have different regions over which they
 * are defined, parameters are estimated only in the intersection of
 * the two image regions. However, the mask region must be a proper sub
 * subset (contained) in the image region.
 * 
 * \sa KrcahEigenToScalarImageFilter
 * 
 * \author: Bryce Besler
 * \ingroup BoneEnhancement
 */
template< typename TInputImage, typename TMaskImage >
class ITK_TEMPLATE_EXPORT DescoteauxEigenToScalarParameterEstimationImageFilter:
public ImageToImageFilter< TInputImage, TInputImage >
{
public:
  ITK_DISALLOW_COPY_AND_ASSIGN(DescoteauxEigenToScalarParameterEstimationImageFilter);

  /** Standard Self type alias */
  using Self = DescoteauxEigenToScalarParameterEstimationImageFilter;
  using Superclass = ImageToImageFilter< TInputImage, TInputImage >;
  using Pointer = SmartPointer< Self >;
  using ConstPointer = SmartPointer< const Self >;

  /** Method for creation through the object factory. */
  itkNewMacro(Self);

  /** Run-time type information (and related methods). */
  itkTypeMacro(DescoteauxEigenToScalarParameterEstimationImageFilter, ImageToImageFilter);

  /** Image related type alias. */
  using InputImagePointer = typename TInputImage::Pointer;
  using InputImageConstPointer = typename TInputImage::ConstPointer;
  using InputRegionType = typename TInputImage::RegionType;
  using InputSizeType = typename TInputImage::SizeType;
  using InputIndexType = typename TInputImage::IndexType;
  using InputPixelType = typename TInputImage::PixelType;
  using InputPixelValueType = typename InputPixelType::ValueType;

  /** Output region definitions */
  using OutputRegionType = InputRegionType;

  /** Mask related type alias. */
  using MaskImagePointer = typename TMaskImage::Pointer;
  using MaskImageConstPointer = typename TMaskImage::ConstPointer;
  using MaskPixelType = typename TMaskImage::PixelType;
  using MaskRegionType = typename TMaskImage::RegionType;

  /** Parameters */
  using RealType = typename NumericTraits< InputPixelValueType >::RealType;
  using RealTypeDecoratedType = SimpleDataObjectDecorator< RealType >;

  /** Methods to set/get the mask image */
  itkSetInputMacro(MaskImage, TMaskImage);
  itkGetInputMacro(MaskImage, TMaskImage);

  /** Setter/getter for background pixel value */
  itkSetMacro(BackgroundValue, MaskPixelType);
  itkGetConstMacro(BackgroundValue, MaskPixelType);

  /** Method setter/getter for Frobenius Norm Weight */
  itkSetMacro(FrobeniusNormWeight, RealType);
  itkGetConstMacro(FrobeniusNormWeight, RealType);

  /** Decorators for parameters so they can be passed as a process object */
  RealTypeDecoratedType * GetAlphaOutput();
  const RealTypeDecoratedType * GetAlphaOutput() const;
  RealTypeDecoratedType * GetBetaOutput();
  const RealTypeDecoratedType * GetBetaOutput() const;
  RealTypeDecoratedType * GetCOutput();
  const RealTypeDecoratedType * GetCOutput() const;

  /** Standard getters for the output parameters */
  RealType GetAlpha() const
  {
    return this->GetAlphaOutput()->Get();
  }
  RealType GetBeta() const
  {
    return this->GetBetaOutput()->Get();
  }
  RealType GetC() const
  {
    return this->GetCOutput()->Get();
  }

#ifdef ITK_USE_CONCEPT_CHECKING
  // Begin concept checking
  itkConceptMacro( InputHaveDimension3Check,
                   ( Concept::SameDimension< TInputImage::ImageDimension, 3u >) );
  // End concept checking
#endif
protected:
  DescoteauxEigenToScalarParameterEstimationImageFilter();
  virtual ~DescoteauxEigenToScalarParameterEstimationImageFilter() {}

  /** Pass the input through unmodified. Do this by Grafting in the AllocateOutputs method. */
  void AllocateOutputs() override;

  /** Initialize some accumulators before the threads run. */
  void BeforeThreadedGenerateData() override;

  /** Do final mean and variance computation from data accumulated in threads. */
  void AfterThreadedGenerateData() override;

  /** Multi-thread version GenerateData. */
  void  ThreadedGenerateData(const OutputRegionType &
                             outputRegionForThread,
                             ThreadIdType threadId) override;

  /** Override since the filter needs all the data for the algorithm */
  void GenerateInputRequestedRegion() override;

  /** Override since the filter produces all of its output */
  void EnlargeOutputRequestedRegion(DataObject *data) override;

  /** Compute Frobenius norm from a fixed array of eigenvalues */
  inline RealType CalculateFrobeniusNorm(InputPixelType pixel);

  void PrintSelf(std::ostream & os, Indent indent) const override;

private:
  /* Parameters */
  RealType      m_FrobeniusNormWeight;
  MaskPixelType m_BackgroundValue;

  /* Arrays for threads */
  Array< RealType >       m_MaxFrobeniusNorm;
}; //end class
} // end namespace

#ifndef ITK_MANUAL_INSTANTIATION
#include "itkDescoteauxEigenToScalarParameterEstimationImageFilter.hxx"
#endif

#endif // itkDescoteauxEigenToScalarParameterEstimationImageFilter_h
