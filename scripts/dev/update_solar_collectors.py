import csv
import os
from eppy.modeleditor import IDF


def update_solar_collectors(csv_path, idd_dir):
  
    # eppy setup
    idd_name = r'Energy+.idd'
    idd_path = os.path.join(idd_dir, idd_name)
    IDF.setiddname(idd_path)

    # create a blank idf, add Version object
    idf_dir = csv_dir
    idf_name = r'SolarCollectors2017.idf'
    idf_path = os.path.join(idf_dir, idf_name)
    open(idf_path, 'w').close()
    idf = IDF(idf_path)
    idf.newidfobject('VERSION')
    idf.save()

    counter = 0

    csv_file = open(csv_path, 'rb')
    reader = csv.reader(csv_file)

    for idx, row in enumerate(reader):

        if idx <= 2 or row[2] == 'Tubular':

            next

        else:

            name = row[0]
            srcc_type = row[2]
            test_flow = row[4]
            area = row[5]
            frta = row[6]
            frul = row[7]
            iam = row[8]

            obj = idf.newidfobject('SolarCollectorPerformance:FlatPlate'.upper())
            obj.Name = name
            obj.Gross_Area = area
            obj.Test_Fluid = 'Water'
            obj.Test_Flow_Rate = float(test_flow)/1000 #kg/s to m3/s
            obj.Test_Correlation_Type = 'Inlet'
            obj.Coefficient_1_of_Efficiency_Equation = frta
            obj.Coefficient_2_of_Efficiency_Equation = frul
            obj.Coefficient_3_of_Efficiency_Equation = 0
            obj.Coefficient_2_of_Incident_Angle_Modifier = iam
            obj.Coefficient_3_of_Incident_Angle_Modifier = 0

            counter += 1

    idf.save()

    print counter     
  