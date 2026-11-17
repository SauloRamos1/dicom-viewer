Feature: View a DICOM image in 3D
  As a user
  I want to see a single image as a relief and a series or multi-frame file as a volume
  So that I can inspect the image structure beyond the flat 2D slice

  Scenario: Heightmap for a single image
    Given a single-frame image is open
    When I switch to the 3D view
    Then a heightmap of the image is built with one vertex per pixel
    And the 3D view has no volume

  Scenario: Volume from a series folder
    Given a folder with the slices of one series
    When I open the folder as a series
    Then the series is a volume with the slice count as depth and the slice spacing as z spacing
    And the 2D view shows the slices as frames

  Scenario: Inconsistent series shows an error
    Given a folder whose slices have different sizes
    When I open the folder as a series
    Then the errors list shows that the slices do not form a consistent volume
    And the previously opened image is still displayed

  Scenario: Multi-frame file opens as a volume
    Given a multi-frame monochrome file
    When I open it
    Then it is a volume with the frame count as depth
