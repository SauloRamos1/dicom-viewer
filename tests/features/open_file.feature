Feature: Open a DICOM file
  As a user
  I want to open a DICOM file and see it displayed correctly
  So that I can inspect the image and understand any problem with a bad file

  Scenario: Open a valid monochrome file
    Given the viewer is showing the empty state
    When I open a valid monochrome DICOM file
    Then the image is shown with the dimensions stored in the file

  Scenario: Open an invalid file reports a typed error
    Given a file that is not a valid DICOM file
    When I open it
    Then loading fails with an error code and a message

  Scenario: Open a DICOM file without pixel data
    Given a valid DICOM file without pixel data
    When I open it
    Then the error names the missing pixel data tag

  Scenario: Open an invalid file
    Given the viewer is showing a previously opened image
    When I open a file that is not a valid DICOM file
    Then the errors list shows the real error instead of the sample errors
    And the previously opened image is still displayed

  Scenario: New resets the viewer
    Given the viewer is showing an image with metadata and options changed
    When I press New
    Then the image, metadata and options are cleared
    And the errors list shows the sample errors again

  Scenario: Change window level and width
    Given a valid monochrome DICOM file is open
    When I change the window level or width
    Then the displayed pixel values change as expected

  Scenario: Open an RGB file
    When I open a valid RGB DICOM file
    Then the image is displayed in color

  # Needs the language menu from step 4.
  @pending
  Scenario: Switch language
    Given the interface language is English
    When I choose Portuguese (BR) from the language menu
    Then the interface strings are shown in Portuguese
